#include <net/tcp.h>
#include <net/derand.h>
#include <net/derand_ops.h>
#include "record_ctrl.h"
#include "derand_recorder.h"
#include "copy_from_sock_init_val.h"
#include "logger.h"

u32 mon_dstip = 0;
u32 mon_ndstip = 0;

static inline int is_valid_recorder(struct derand_recorder *rec){
	int idx = rec - (struct derand_recorder*)record_ctrl.addr;
	return idx >= 0 && idx < record_ctrl.max_sock;
}

#if ADVANCED_EVENT_ENABLE
static inline void record_advanced_event(const struct sock *sk, u8 func_num, u8 loc, u8 fmt, int n, ...){
	u32 i, j;
	va_list vl;
	struct AdvancedEventQ* aeq = &((struct derand_recorder*)(sk->recorder))->aeq;

	// store ae header
	aeq->v[get_aeq_idx(aeq->t)] = get_ae_hdr_u32(func_num, loc, n, fmt);
	// store data
	va_start(vl, n);
	for (i = 1, j = 1<<(n-1); j; i++, j>>=1){
		if (fmt & j){
			u64 a = va_arg(vl, u64);
			aeq->v[get_aeq_idx(aeq->t + i)] = (u32)(a & 0xffffffff);
			i++;
			aeq->v[get_aeq_idx(aeq->t + i)] = (u32)(a >> 32);
		}else{
			aeq->v[get_aeq_idx(aeq->t + i)] = va_arg(vl, int);
		}
	}
	va_end(vl);
	wmb();
	aeq->t += i;
}
#endif

#if DERAND_DEBUG
static inline void add_geq(struct GeneralEventQ *geq, u32 type, u64 data){
	struct GeneralEvent *ge = &geq->v[get_geq_idx(geq->t)];
	ge->type = type;
	*(u64*)(ge->data) = data;
	wmb();
	geq->t++;
}
#endif /* DERAND_DEBUG */

// allocate memory for a socket
static void* derand_alloc_mem(void){
	void* ret = NULL;
	int n;

	spin_lock_bh(&record_ctrl.lock);
	if (record_ctrl.top == 0)
		goto out;

	n = record_ctrl.stack[--record_ctrl.top];
	ret = record_ctrl.addr + n; // * sizeof(struct derand_recorder);
out:
	spin_unlock_bh(&record_ctrl.lock);
	return ret;
}

/* create a derand_recorder.
 * Called when a connection is successfully built
 * i.e., after receiving SYN-ACK at client and after receiving ACK at server.
 * So this function is called in bottom-half, so we must not block*/
static void recorder_create(struct sock *sk, struct sk_buff *skb, int mode){
	int i;

	// create derand_recorder
	struct derand_recorder *rec = derand_alloc_mem();
	if (!rec){
		printk("[recorder_create] sport = %hu, dport = %hu, fail to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, record_ctrl.top);
		goto out;
	}
	printk("[recorder_create] sport = %hu, dport = %hu, succeed to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, record_ctrl.top);
	sk->recorder = rec;

	// set broken to 0;
	rec->broken = 0;
	rec->alert = 0;

	// record 4 tuples
	rec->sip = inet_sk(sk)->inet_saddr;
	rec->dip = inet_sk(sk)->inet_daddr;
	rec->sport = inet_sk(sk)->inet_sport;
	rec->dport = inet_sk(sk)->inet_dport;

	// init PktIdx
	if (mode == 0){ // ipid is consecutive at this point only for server side
		rec->pkt_idx.init_ipid = rec->pkt_idx.last_ipid = ntohs(ip_hdr(skb)->id);
		rec->pkt_idx.first = 0;
	}else { // in our custom kernel, the first data/ack packet has ipid 1
		rec->pkt_idx.init_ipid = rec->pkt_idx.last_ipid = 0; // we set last_ipid to 0, so the next data/ack pkt should just be 0 +1 = 1
		rec->pkt_idx.first = 0;
		/*
		// for client side, the next packet carries the first valid ipid
		rec->pkt_idx.first = 1;
		*/
	}
	rec->pkt_idx.idx = 0;
	rec->pkt_idx.fin_seq = 0;
	rec->pkt_idx.fin = 0;

	// init variables
	rec->seq = 0;
	atomic_set(&rec->sockcall_id, 0);
	rec->evt_h = rec->evt_t = 0;
	rec->sc_h = rec->sc_t = 0;
	rec->dpq.h = rec->dpq.t = 0;
	rec->jf.h = rec->jf.t = rec->jf.idx_delta = rec->jf.last_jiffies = 0;
	rec->mpq.h = rec->mpq.t = 0;
	rec->maq.h = rec->maq.t = rec->maq.idx_delta = rec->maq.last_v = 0;
	rec->n_sockets_allocated = 0;
	rec->msq.h = rec->msq.t = 0;
	for (i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		rec->ebq[i].h = rec->ebq[i].t = 0;
	rec->siqq.h = rec->siqq.t = 0;
	#if DERAND_DEBUG
	rec->geq.h = rec->geq.t = 0;
	#endif
	#if ADVANCED_EVENT_ENABLE
	rec->aeq.h = rec->aeq.t = 0;
	#endif
	#if COLLECT_TX_STAMP
	rec->tsq.h = rec->tsq.t = 0;
	#endif
	#if COLLECT_RX_STAMP
	rec->rsq.h = rec->rsq.t = 0;
	#endif
	if (mode == 0)
		copy_from_server_sock(sk); // copy sock init data
	else 
		copy_from_client_sock(sk);
	rec->mode = mode;
	wmb();
	rec->recorder_id++;
out:
	return;
}

static void server_recorder_create(struct sock *sk, struct sk_buff *skb){
	uint16_t sport = ntohs(inet_sk(sk)->inet_sport);
	u32 dip = inet_sk(sk)->inet_daddr;
	if (dip != mon_ndstip && (mon_dstip == 0 || dip == mon_dstip) && ((sport >= 60000 && sport <= 60003) || sport == 50010)){
		printk("server dip = %08x, sport = %hu, dport = %hu, creating recorder\n", ntohl(dip), ntohs(inet_sk(sk)->inet_sport), ntohs(inet_sk(sk)->inet_dport));
		recorder_create(sk, skb, 0);
	}
}
static void client_recorder_create(struct sock *sk, struct sk_buff *skb){
	uint16_t dport = ntohs(inet_sk(sk)->inet_dport);
	u32 dip = inet_sk(sk)->inet_daddr;
	if (dip != mon_ndstip && (mon_dstip == 0 || dip == mon_dstip) && ((dport >= 60000 && dport <= 60003) || dport == 50010)){
	//if (inet_sk(sk)->inet_dport == 0x8913){ // port 5001
		printk("client dip = %08x, sport = %hu, dport = %hu, creating recorder\n", ntohl(dip), inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		recorder_create(sk, skb, 1);
	}
}

/* this function should be called in thread_safe environment */
static inline void new_event(struct sock *sk, u32 type){
	struct derand_recorder *rec = (struct derand_recorder*)sk->recorder;
	u32 seq;
	u32 idx;
	if (!rec)
		return;

	// increment the seq #
	seq = rec->seq++;

	// enqueue the new event
	idx = get_evt_q_idx(rec->evt_t);
	#if DERAND_DEBUG
	//add_geq(&rec->geq, 0, type);
	rec->evts[idx] = (struct derand_event){.seq = seq, .type = type, .dbg_data = tcp_sk(sk)->write_seq};
	#else
	rec->evts[idx] = (struct derand_event){.seq = seq, .type = type};
	#endif
	#if GET_EVENT_STAMP
	rec->evts[idx].ts = ktime_get().tv64;
	#endif
	#if GET_CWND
	rec->evts[idx].cwnd = tcp_sk(sk)->snd_cwnd;
	rec->evts[idx].ssthresh = tcp_sk(sk)->snd_ssthresh;
	rec->evts[idx].is_cwnd_limited = tcp_sk(sk)->is_cwnd_limited;
	#endif
	wmb();
	rec->evt_t++;
}

/* destruct a derand_recorder.
 */
static void recorder_destruct(struct sock *sk){
	struct derand_recorder* rec = sk->recorder;
	if (!rec){
		//printk("[recorder_destruct] sport = %hu, dport = %hu, recorder is NULL.\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		return;
	}

	// add a finish event
	new_event(sk, EVENT_TYPE_FINISH);

	// update recorder_id
	rec->recorder_id++;

	// recycle this recorder
	spin_lock_bh(&record_ctrl.lock);
	record_ctrl.stack[record_ctrl.top++] = (rec - record_ctrl.addr); // / sizeof(struct derand_recorder);
	spin_unlock_bh(&record_ctrl.lock);

	// remove the recorder from sk
	sk->recorder = NULL;
	printk("[recorder_destruct] sport = %hu, dport = %hu, succeed to destruct a recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, record_ctrl.top);
}

/******************************************
 * sockcall
 *****************************************/
static inline void update_sc_t(struct derand_recorder *rec, int sc_id){
	// make sure sockcall data is written, before we inc sc_t
	wmb();
	if (rec->sc_t != sc_id){
		// This means there are other concurrent sockcall getting sc_id, and get a smaller sc_id than this
		// We should wait for them to inc sc_t, before we inc sc_t. Otherwise the user recorder may record incomplete data for other sockcalls
		atomic_t *sc_t = (atomic_t*)&rec->sc_t;
		u32 cnt = 1;
		for (;atomic_read(sc_t) != sc_id; cnt++){
			if ((cnt & 0x0fffffff) == 0)
				printk("long wait for sc_t: %u sc_id %d\n", atomic_read(sc_t), sc_id);
		}
	}
	atomic_inc((atomic_t*)&rec->sc_t);
}
static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)]; // we have to store it at idx sc_id, not sc_t: because 2 sockcall may be concurrent, so sc_t may not = sc_id
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_SENDMSG;
	rec_sc->sendmsg.flags = msg->msg_flags;
	rec_sc->sendmsg.size = size;
	rec_sc->thread_id = (u64)current;
	// update sc_t
	update_sc_t(rec, sc_id);
	// return sockcall ID 
	return sc_id;
}

#if 0
static u32 new_sendpage(struct sock *sk, int offset, size_t size, int flags){
	struct derand_recorder* rec = sk->recorder;
	if (!rec)
		return 0;
	printk("[DERAND] %hu %hu: tcp_sendpage %d %lu %x!!!\n", ntohs(rec->sport), ntohs(rec->dport), offset, size, flags);
	return 0;
}
#endif

static u32 new_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock, int flags, int *addr_len){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)];
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_RECVMSG;
	rec_sc->recvmsg.flags = nonblock | flags;
	rec_sc->recvmsg.size = len;
	rec_sc->thread_id = (u64)current;
	// update sc_t
	update_sc_t(rec, sc_id);
	// return sockcall ID
	return sc_id;
}

static u32 new_splice_read(struct sock *sk, size_t len, unsigned int flags){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)];
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_SPLICE_READ;
	rec_sc->splice_read.flags = flags;
	rec_sc->splice_read.size = len;
	rec_sc->thread_id = (u64)current;
	// update sc_t
	update_sc_t(rec, sc_id);
	// return sockcall ID
	return sc_id;
}

static u32 new_close(struct sock *sk, long timeout){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;
	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)];
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_CLOSE;
	rec_sc->close.timeout = timeout;
	rec_sc->thread_id = (u64)current;
	// update sc_t
	update_sc_t(rec, sc_id);
	// return sockcall ID
	return sc_id;
}

static u32 new_setsockopt(struct sock *sk, int level, int optname, char __user *optval, unsigned int optlen){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;
	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)];
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_SETSOCKOPT;
	if ((u32)level > 255 || (u32)optname > 255 || (u32)optlen > 12){ // use u32 so that negative values are also considered
		rec_sc->setsockopt.level = rec_sc->setsockopt.optname = rec_sc->setsockopt.optlen = 255;
		rec->broken |= BROKEN_UNSUPPORTED_SOCKOPT;
	}else {
		rec_sc->setsockopt.level = level;
		rec_sc->setsockopt.optname = optname;
		rec_sc->setsockopt.optlen = optlen;
		copy_from_user(rec_sc->setsockopt.optval, optval, optlen);
	}
	// update sc_t
	update_sc_t(rec, sc_id);
	// return sockcall ID
	return sc_id;
}

/****************************************
 * events
 * These hooks are called right after getting the spin-lock
 ***************************************/
static void sockcall_lock(struct sock *sk, u32 sc_id){
	new_event(sk, sc_id + DERAND_SOCK_ID_BASE);
}

/* incoming packets do not need to record their seq # */
static void incoming_pkt(struct sock *sk){
	if (!sk->recorder)
		return;
	((struct derand_recorder*)sk->recorder)->seq++;
}

static void write_timer(struct sock *sk){
	new_event(sk, EVENT_TYPE_WRITE_TIMEOUT);
}

static void delack_timer(struct sock *sk){
	new_event(sk, EVENT_TYPE_DELACK_TIMEOUT);
}

static void keepalive_timer(struct sock *sk){
	new_event(sk, EVENT_TYPE_KEEPALIVE_TIMEOUT);
}

static void tasklet(struct sock *sk){
	new_event(sk, EVENT_TYPE_TASKLET);
}

/********************************************
 * monitor network action: drop & ecn
 *******************************************/
#if COLLECT_RX_STAMP
static inline void rx_stamp(struct derand_recorder *rec){
	u64 clock = local_clock();
	rec->rsq.v[get_rsq_idx(rec->rsq.t)] = clock;
	wmb();
	rec->rsq.t++;
}
#endif

void mon_net_action(struct sock *sk, struct sk_buff *skb){
	struct derand_recorder *rec = (struct derand_recorder*)sk->recorder;
	u16 ipid, gap;
	u32 i, idx;
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);
	if (!rec)
		return;

	#if COLLECT_RX_STAMP
	rx_stamp(rec);
	#endif

	// if this packet ackes our fin, skip. ipid from the other side may not be consecutive after acking our fin
	if (rec->pkt_idx.fin && ntohl(tcph->ack_seq) == rec->pkt_idx.fin_seq + 1)
		return;
	
	ipid = ntohs(iph->id);

	if (rec->pkt_idx.first){
		// this is the first valid ipid for client side, record and return
		rec->pkt_idx.init_ipid = rec->pkt_idx.last_ipid = ipid;
		rec->pkt_idx.first = 0;
		return;
	}

	gap = get_pkt_idx_gap(&rec->pkt_idx, ipid);
	idx = rec->pkt_idx.idx;

	// record drops
	for (i = 1; i < gap; i++){
		rec->dpq.v[get_drop_q_idx(rec->dpq.t)] = idx + i;
		wmb();
		rec->dpq.t++;
	}
	update_pkt_idx(&rec->pkt_idx, ipid);
}

void record_fin_seq(struct sock *sk){
	struct derand_recorder *rec = (struct derand_recorder*)sk->recorder;
	if (!rec)
		return;
	rec->pkt_idx.fin_v64 = tcp_sk(sk)->write_seq | (0x100000000);
}

/***********************************************
 * shared variables
 **********************************************/
static void _read_jiffies(const struct sock *sk, unsigned long v, int id){
	struct derand_recorder* rec = sk->recorder;
	struct jiffies_q *jf;
	if (!is_valid_recorder(rec))
		return;
	jf = &rec->jf;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)sk->recorder)->geq, 1, id);
	#endif
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -1, id, 0b0, 1, jf->t); // jiffies: type=-1, loc=id, fmt=0, data=jf->t
	#endif
	if (jf->t == 0){ // this is the first jiffies read
		jf->v[0].init_jiffies = v;
		jf->last_jiffies = v;
		jf->idx_delta = 0;
		wmb();
		jf->t = 1;
	}else if (v != jf->last_jiffies) {
		union jiffies_rec *jf_rec = &jf->v[get_jiffies_q_idx(jf->t)];
		jf_rec->jiffies_delta = v - jf->last_jiffies;
		jf_rec->idx_delta = jf->idx_delta + 1;
		jf->last_jiffies = v;
		jf->idx_delta = 0;
		wmb();
		jf->t++;
	}else
		jf->idx_delta++;
}
static void read_jiffies(const struct sock *sk, unsigned long v, int id){
	_read_jiffies(sk, v, id + 100);
}
static void read_tcp_time_stamp(const struct sock *sk, unsigned long v, int id){
	_read_jiffies(sk, v, id);
}

static inline void push_memory_pressure_q(struct memory_pressure_q *q, bool v){
	u32 idx = get_memory_pressure_q_idx(q->t);
	u32 bit_idx = idx & 31, arr_idx = idx / 32;
	if (bit_idx == 0)
		q->v[arr_idx] = (u32)v;
	else
		q->v[arr_idx] |= ((u32)v) << bit_idx;
	wmb();
	q->t++;
}
static void record_tcp_under_memory_pressure(const struct sock *sk, bool ret){
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 2, ret);
	#endif
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -2, 0, 0b0, 1, (int)ret); // mpq: type=-2, loc=0, fmt=0, data=ret
	#endif
	push_memory_pressure_q(&((struct derand_recorder*)(sk->recorder))->mpq, ret);
}

static void record_sk_under_memory_pressure(const struct sock *sk, bool ret){
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 2, ret);
	#endif
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -2, 0, 0b0, 1, (int)ret); // mpq: type=-2, loc=0, fmt=0, data=ret
	#endif
	push_memory_pressure_q(&((struct derand_recorder*)(sk->recorder))->mpq, ret);
}

static void record_sk_memory_allocated(const struct sock *sk, long ret){
	struct memory_allocated_q *maq = &((struct derand_recorder*)sk->recorder)->maq;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 3, ret);
	#endif
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -3, 0, 0b0, 1, maq->t); // maq: type=-3, loc=0, fmt=0b0, data=maq->t
	#endif
	if (maq->t == 0){ // this is the first read to memory_allocated
		maq->v[0].init_v = ret;
		maq->last_v = ret;
		maq->idx_delta = 0;
		wmb();
		maq->t = 1;
	}else if (ret != maq->last_v){
		union memory_allocated_rec *ma_rec = &maq->v[get_memory_allocated_q_idx(maq->t)];
		ma_rec->v_delta = (s32)(ret - maq->last_v);
		ma_rec->idx_delta = maq->idx_delta + 1;
		maq->last_v = ret;
		maq->idx_delta = 0;
		wmb();
		maq->t++;
	}else 
		maq->idx_delta++;
}

static void record_sk_sockets_allocated_read_positive(struct sock *sk, int ret){
	struct derand_recorder *rec = sk->recorder;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 4, ret);
	#endif
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -4, 0, 0b0, 1, 0); // type=-4, loc=0, fmt=0b00, data=0
	#endif
	rec->n_sockets_allocated++;
}

static void record_skb_mstamp_get(struct sock *sk, struct skb_mstamp *cl, int loc){
	struct mstamp_q *msq = &((struct derand_recorder*)sk->recorder)->msq;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 5, loc);
	#endif
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -5, 0, 0b0, 1, msq->t); // msq: type=-5, loc=0, fmt=0b0, data=msq->t
	#endif
	msq->v[get_mstamp_q_idx(msq->t)] = *cl;
	wmb();
	msq->t++;
}

static inline void push_effect_bool_q(struct effect_bool_q *q, bool v){
	u32 idx = get_effect_bool_q_idx(q->t);
	u32 bit_idx = idx & 31, arr_idx = idx / 32;
	if (bit_idx == 0)
		q->v[arr_idx] = (u32)v;
	else
		q->v[arr_idx] |= ((u32)v) << bit_idx;
	wmb();
	q->t++;
}
static void record_effect_bool(const struct sock *sk, int loc, bool v){
	#if DERAND_DEBUG
	if (loc != 0) // loc 0 is not serializable among all events, but just within incoming packets
		add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 6 + loc, v);
	#endif
	#if ADVANCED_EVENT_ENABLE
	if (loc != 0) // loc 0 is not serializable among all events, but just within incoming packets
		record_advanced_event(sk, -6, loc, 0b0, 1, ((struct derand_recorder*)sk->recorder)->ebq[loc].t); // ebq: type=-6, loc=loc, fmt=0b0, data=ebq[loc].t
	#endif
	push_effect_bool_q(&((struct derand_recorder*)sk->recorder)->ebq[loc], v);
}

static void record_skb_still_in_host_queue(const struct sock *sk, bool ret){
	struct SkbInQueueQ *siqq = &((struct derand_recorder*)sk->recorder)->siqq;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -7, 0, 0b0, 1, siqq->t); // siqq: type=-7, loc=0, fmt=0b0, data=siqq->t
	#endif
	siqq->v[get_siq_q_idx(siqq->t)] = ret;
	wmb();
	siqq->t++;
}

#if DERAND_DEBUG
static void record_general_event(const struct sock *sk, int loc, u64 data){
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, loc + 1000, data);
}
#endif

#if COLLECT_TX_STAMP
static void tx_stamp(const struct sk_buff *skb){
	struct sock* sk = skb->sk;
	if (!sk)
		return;
	struct derand_recorder *rec = (struct derand_recorder*)sk->recorder;
	if (!rec || !is_valid_recorder(rec))
		return;
	u64 clock = local_clock();
	rec->tsq.v[get_tsq_idx(rec->tsq.t)] = clock;
	wmb();
	rec->tsq.t++;
}
#endif

/******************************
 * alert
 *****************************/
static void record_alert(const struct sock *sk, int loc){
	struct derand_recorder *rec = (struct derand_recorder*)sk->recorder;
	if (rec)
		rec->alert |= 1 << loc;
}

int bind_record_ops(void){
	derand_record_ops.recorder_destruct = recorder_destruct;
	derand_record_ops.new_sendmsg = new_sendmsg;
	//derand_record_ops.new_sendpage = new_sendpage;
	derand_record_ops.new_recvmsg = new_recvmsg;
	derand_record_ops.new_splice_read = new_splice_read;
	derand_record_ops.new_close = new_close;
	derand_record_ops.new_setsockopt = new_setsockopt;
	derand_record_ops.sockcall_lock = sockcall_lock;
	derand_record_ops.incoming_pkt = incoming_pkt;
	derand_record_ops.write_timer = write_timer;
	derand_record_ops.delack_timer = delack_timer;
	derand_record_ops.keepalive_timer = keepalive_timer;
	derand_record_ops.tasklet = tasklet;
	derand_record_ops.mon_net_action = mon_net_action;
	derand_record_ops.record_fin_seq = record_fin_seq;
	derand_record_ops.read_jiffies = read_jiffies;
	derand_record_ops.read_tcp_time_stamp = read_tcp_time_stamp; // store jiffies and tcp_time_stamp together
	derand_record_ops.tcp_under_memory_pressure = record_tcp_under_memory_pressure;
	derand_record_ops.sk_under_memory_pressure = record_sk_under_memory_pressure;
	derand_record_ops.sk_memory_allocated = record_sk_memory_allocated;
	derand_record_ops.sk_sockets_allocated_read_positive = record_sk_sockets_allocated_read_positive;
	derand_record_ops.skb_mstamp_get = record_skb_mstamp_get;
	derand_record_ops.record_skb_still_in_host_queue = record_skb_still_in_host_queue;
	#if DERAND_DEBUG
	derand_record_ops.general_event = record_general_event;
	#endif
	#if COLLECT_TX_STAMP
	derand_record_ops.tx_stamp = tx_stamp;
	#endif
	#if ADVANCED_EVENT_ENABLE
	advanced_event = record_advanced_event;
	#endif
	derand_record_effect_bool = record_effect_bool;
	derand_record_alert = record_alert;

	/* The recorder_create functions must be bind last, because they are the enabler of record */
	derand_record_ops.server_recorder_create = server_recorder_create;
	derand_record_ops.client_recorder_create = client_recorder_create;
	return 0;
}
void unbind_record_ops(void){
	derand_record_effect_bool = NULL;
	derand_record_alert = NULL;
	#if ADVANCED_EVENT_ENABLE
	advanced_event = NULL;
	#endif
	derand_record_ops = derand_record_ops_default;
}
