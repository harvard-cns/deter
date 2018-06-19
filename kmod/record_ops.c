#include <net/tcp.h>
#include <net/derand.h>
#include <net/derand_ops.h>
#include "record_ctrl.h"
#include "derand_recorder.h"
#include "copy_from_sock_init_val.h"

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
static void recorder_create(struct sock *sk, int server_side){
	int i;

	// create derand_recorder
	struct derand_recorder *rec = derand_alloc_mem();
	if (!rec){
		printk("[recorder_create] sport = %hu, dport = %hu, fail to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, record_ctrl.top);
		goto out;
	}
	printk("[recorder_create] sport = %hu, dport = %hu, succeed to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, record_ctrl.top);
	sk->recorder = rec;

	// record 4 tuples
	rec->sip = inet_sk(sk)->inet_saddr;
	rec->dip = inet_sk(sk)->inet_daddr;
	rec->sport = inet_sk(sk)->inet_sport;
	rec->dport = inet_sk(sk)->inet_dport;

	// init variables
	rec->seq = 0;
	atomic_set(&rec->sockcall_id, 0);
	rec->evt_h = rec->evt_t = 0;
	rec->sc_h = rec->sc_t = 0;
	rec->jf.h = rec->jf.t = rec->jf.idx_delta = rec->jf.last_jiffies = 0;
	rec->mpq.h = rec->mpq.t = 0;
	rec->maq.h = rec->maq.t = rec->maq.idx_delta = rec->maq.last_v = 0;
	rec->n_sockets_allocated = 0;
	rec->msq.h = rec->msq.t = 0;
	for (i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		rec->ebq[i].h = rec->ebq[i].t = 0;
	#if DERAND_DEBUG
	rec->geq.h = rec->geq.t = 0;
	#endif
	if (server_side)
		copy_from_server_sock(sk); // copy sock init data
	rec->recorder_id++;
out:
	return;
}

static void server_recorder_create(struct sock *sk){
	uint16_t sport = ntohs(inet_sk(sk)->inet_sport);
	if ((sport >= 60000 && sport <= 60003) || sport == 50010){
		printk("server sport = %hu, dport = %hu, creating recorder\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		recorder_create(sk, 1);
	}
}
static void client_recorder_create(struct sock *sk){
	uint16_t dport = ntohs(inet_sk(sk)->inet_dport);
	if ((dport >= 60000 && dport <= 60003) || dport == 50010){
	//if (inet_sk(sk)->inet_dport == 0x8913){ // port 5001
		printk("client sport = %hu, dport = %hu, creating recorder\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		recorder_create(sk, 0);
	}
}

static inline void new_event(struct sock *sk, u32 type){
	struct derand_recorder *rec = (struct derand_recorder*)sk->recorder;
	u32 seq;
	if (!rec)
		return;

	// increment the seq #
	seq = rec->seq++;

	// enqueue the new event
	#if DERAND_DEBUG
	add_geq(&rec->geq, 0);
	rec->evts[get_evt_q_idx(rec->evt_t)] = (struct derand_event){.seq = seq, .type = type, .dbg_data = tcp_sk(sk)->write_seq};
	#else
	rec->evts[get_evt_q_idx(rec->evt_t)] = (struct derand_event){.seq = seq, .type = type};
	#endif
	rec->evt_t++;
}

/* destruct a derand_recorder.
 */
static void recorder_destruct(struct sock *sk){
	struct derand_recorder* rec = sk->recorder;
	if (!rec){
		printk("[recorder_destruct] sport = %hu, dport = %hu, recorder is NULL.\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
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

static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
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
	rec_sc->type = DERAND_SOCKCALL_TYPE_SENDMSG;
	rec_sc->sendmsg.flags = msg->msg_flags;
	rec_sc->sendmsg.size = size;
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
	rec_sc->recvmsg.flags = nonblock & msg->msg_flags;
	rec_sc->recvmsg.size = len;
	// return sockcall ID 
	return sc_id;
}

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

static void read_jiffies(const struct sock *sk, unsigned long v, int id){
	struct jiffies_q *jf = &((struct derand_recorder*)sk->recorder)->jf;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)sk->recorder)->geq, 1);
	#endif
	if (jf->t == 0){ // this is the first jiffies read
		jf->v[0].init_jiffies = v;
		jf->last_jiffies = v;
		jf->idx_delta = 0;
		jf->t = 1;
	}else if (v != jf->last_jiffies) {
		union jiffies_rec *jf_rec = &jf->v[get_jiffies_q_idx(jf->t)];
		jf_rec->jiffies_delta = v - jf->last_jiffies;
		jf_rec->idx_delta = jf->idx_delta + 1;
		jf->last_jiffies = v;
		jf->idx_delta = 0;
		jf->t++;
	}else
		jf->idx_delta++;
}

static void record_tcp_under_memory_pressure(const struct sock *sk, bool ret){
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 2);
	#endif
	push_memory_pressure_q(&((struct derand_recorder*)(sk->recorder))->mpq, ret);
}

static void record_sk_under_memory_pressure(const struct sock *sk, bool ret){
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 2);
	#endif
	push_memory_pressure_q(&((struct derand_recorder*)(sk->recorder))->mpq, ret);
}

static void record_sk_memory_allocated(const struct sock *sk, long ret){
	struct memory_allocated_q *maq = &((struct derand_recorder*)sk->recorder)->maq;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 3);
	#endif
	if (maq->t == 0){ // this is the first read to memory_allocated
		maq->v[0].init_v = ret;
		maq->last_v = ret;
		maq->idx_delta = 0;
		maq->t = 1;
	}else if (ret != maq->last_v){
		union memory_allocated_rec *ma_rec = &maq->v[get_memory_allocated_q_idx(maq->t)];
		ma_rec->v_delta = (s32)(ret - maq->last_v);
		ma_rec->idx_delta = maq->idx_delta + 1;
		maq->last_v = ret;
		maq->idx_delta = 0;
		maq->t++;
	}else 
		maq->idx_delta++;
}

static void record_sk_sockets_allocated_read_positive(struct sock *sk, int ret){
	struct derand_recorder *rec = sk->recorder;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 4);
	#endif
	rec->n_sockets_allocated++;
}

static void record_skb_mstamp_get(struct sock *sk, struct skb_mstamp *cl, int loc){
	struct mstamp_q *msq = &((struct derand_recorder*)sk->recorder)->msq;
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 5);
	#endif
	msq->v[get_mstamp_q_idx(msq->t)] = *cl;
	msq->t++;
}

static void record_effect_bool(const struct sock *sk, int loc, bool v){
	#if DERAND_DEBUG
	add_geq(&((struct derand_recorder*)(sk->recorder))->geq, 6 + loc);
	#endif
	push_effect_bool_q(&((struct derand_recorder*)sk->recorder)->ebq[loc], v);
}

int bind_record_ops(void){
	derand_record_ops.recorder_destruct = recorder_destruct;
	derand_record_ops.new_sendmsg = new_sendmsg;
	//derand_record_ops.new_sendpage = new_sendpage;
	derand_record_ops.new_recvmsg = new_recvmsg;
	derand_record_ops.sockcall_lock = sockcall_lock;
	derand_record_ops.incoming_pkt = incoming_pkt;
	derand_record_ops.write_timer = write_timer;
	derand_record_ops.delack_timer = delack_timer;
	derand_record_ops.keepalive_timer = keepalive_timer;
	derand_record_ops.tasklet = tasklet;
	derand_record_ops.read_jiffies = read_jiffies;
	derand_record_ops.read_tcp_time_stamp = read_jiffies; // store jiffies and tcp_time_stamp together
	derand_record_ops.tcp_under_memory_pressure = record_tcp_under_memory_pressure;
	derand_record_ops.sk_under_memory_pressure = record_sk_under_memory_pressure;
	derand_record_ops.sk_memory_allocated = record_sk_memory_allocated;
	derand_record_ops.sk_sockets_allocated_read_positive = record_sk_sockets_allocated_read_positive;
	derand_record_ops.skb_mstamp_get = record_skb_mstamp_get;
	derand_record_effect_bool = record_effect_bool;

	/* The recorder_create functions must be bind last, because they are the enabler of record */
	derand_record_ops.server_recorder_create = server_recorder_create;
	derand_record_ops.client_recorder_create = client_recorder_create;
	return 0;
}
void unbind_record_ops(void){
	derand_record_effect_bool = NULL;
	derand_record_ops = derand_record_ops_default;
}
