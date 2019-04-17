#include <net/tcp.h>
#include <linux/string.h>
#include <net/derand.h>
#include <net/derand_ops.h>
#include "deter_recorder.h"
#include "mem_block_ops.h"
#include "copy_from_sock_init_val.h"
#include "logger.h"
#include "record_shmem.h"

u32 mon_dstip = 0;
u32 mon_ndstip = 0;

static inline int is_valid_recorder(struct DeterRecorder *rec){
	int idx = rec - (struct DeterRecorder*) shmem.addr->recorder;
	return idx >= 0 && idx < N_RECORDER;
}

static inline u32 rec2idx(struct DeterRecorder* rec){
	return (u32)(rec - shmem.addr->recorder);
}
static inline u32 mb2idx(struct MemBlock *mb){
	return (u32)(mb - shmem.addr->mem_block);
}

// TODO: potential concurrency bug here: this is a multi-consumer queue
//		 Do a test of h < t before the atomic add would help, because it solves 2 concurrent consumer case, which is already rare, and >2 concurrent consumer is super rare...
static void* deter_alloc_recorder(void){
	atomic_t *h = (atomic_t*)&shmem.addr->free_rec_ring.h;
	u32 ring_idx = atomic_add_return(1, h) - 1; // use atomic_add_return, so concurrent callers are guaranteed to get diff ring_idx
	u32 rec_idx;
	// if not enough free recorder, dec h back.
	if (ring_idx >= (u32)shmem.addr->free_rec_ring.t){// it is fine to directly read t here (not atomic_read), because this won't cause error
		atomic_dec(h);
		return NULL;
	}
	// we know this slot (ring_idx) in the ring is valid (won't be written by user), because the ring is large enough to contain N_RECORDER recorder
	rec_idx = shmem.addr->free_rec_ring.v[get_rec_ring_idx(ring_idx)];
	return &shmem.addr->recorder[rec_idx];
}

// TODO: potential concurrency bug here: this is a multi-consumer queue
//		 Do a test of h < t before the atomic add would help, because it solves 2 concurrent consumer case, which is already rare, and >2 concurrent consumer is super rare...
static inline struct MemBlock* get_free_mem_block(void){
	struct FreeMemBlockRing *ring = &shmem.addr->free_mb_ring;
	atomic_t *h = (atomic_t*)&ring->h;
	u32 ring_idx = atomic_add_return(1, h) - 1; // use atomic_add_return, so concurrent callers are guaranteed to get diff ring_idx
	u32 mb_idx;
	// if not enough free MemBlock, dec h back.
	if (ring_idx >= (u32)atomic_read((atomic_t*)&ring->t)){
		atomic_dec(h);
		return NULL;
	}
	// we know this slot (ring_idx) in the ring is valid (won't be written by user), because the ring is large enough to contain N_MEM_BLOCK mb
	mb_idx = ring->v[get_free_mb_ring_idx(ring_idx)];
	return &shmem.addr->mem_block[mb_idx];
}

static inline struct MemBlock* get_and_init_mem_block(struct DeterRecorder* rec, u8 type){
	struct MemBlock* mb = get_free_mem_block();
	if (mb){
		mb->len = 0;
		mb->type = type;
		mb->rec_id = rec2idx(rec);
		rec->used_mb++;
	}
	return mb;
}

static inline void put_done_mem_block(struct MemBlock* mb){
	struct DoneMemBlockRing *ring = &shmem.addr->done_mb_ring;
	atomic_t *t_mp = (atomic_t*)&ring->t_mp;
	u32 ring_idx = atomic_add_return(1, t_mp) - 1;
	// there is not need to check if the ring has enough space, because the ring is large enough
	ring->v[get_done_mb_ring_idx(ring_idx)] = mb2idx(mb);
	while (atomic_read((atomic_t*)&ring->t) != ring_idx); // if the condition is true, there are other concurrent putters that get lower ring_idx; wait for them to finish
	atomic_inc((atomic_t*)&ring->t);
}

/* 
 * Set of push_* functions that first check mb space, put done and get a new mb if necessary, and push data to the mb.
 * Functions includes:
 *     push_bit(struct DeterRecorder*rec, struct MemBlock** cur, u8 type, u8 x)
 *     push_u8(struct DeterRecorder*rec, struct MemBlock** cur, u8 type, u8 x)
 *     push_u16(struct DeterRecorder*rec, struct MemBlock** cur, u8 type, u16 x)
 *     push_u32(struct DeterRecorder*rec, struct MemBlock** cur, u8 type, u32 x)
 *     push_u64(struct DeterRecorder*rec, struct MemBlock** cur, u8 type, u64 x)
 *     push_nbyte(struct DeterRecorder*rec, struct MemBlock** cur, u8 type, u32 nbyte, void* addr)
 */
#define DEFINE_PUSH_BLOCK_FUNC(name, tp)\
static inline void push_##name(struct DeterRecorder* rec, struct MemBlock** cur, u8 type, tp x){\
	if (!check_space_##name##_block(*cur)){ \
		put_done_mem_block(*cur); \
		while ((*cur = get_and_init_mem_block(rec, type)) == NULL); \
	} \
	push_##name##_block((*cur), x); \
}

DEFINE_PUSH_BLOCK_FUNC(bit, u8);
DEFINE_PUSH_BLOCK_FUNC(u8, u8);
DEFINE_PUSH_BLOCK_FUNC(u16, u16);
DEFINE_PUSH_BLOCK_FUNC(u32, u32);
DEFINE_PUSH_BLOCK_FUNC(u64, u64);

static inline void push_nbyte(struct DeterRecorder *rec, struct MemBlock** cur, u8 type, u32 nbyte, void* addr){
	if (!check_space_nbyte_block((*cur), nbyte)){
		put_done_mem_block(*cur);
		while ((*cur = get_and_init_mem_block(rec, type)) == NULL);
	}
	push_nbyte_block(*cur, nbyte, addr);
}

/* 
 * n: number of MemBlock to get
 * v: the return vector of free MemBlock (this vector memory should be allocated by the caller)
 * TODO: potential concurrency bug here, same as get_free_mem_block above
 */
static inline bool get_n_free_mem_block(u32 n, struct MemBlock **v){
	struct FreeMemBlockRing *mb_ring = &shmem.addr->free_mb_ring;
	atomic_t *h = (atomic_t*)&mb_ring->h;
	u32 ring_idx = atomic_add_return(n, h) - n; // use atomic_add_return, so concurrent callers are guaranteed to get diff ring_idx
	u32 i;
	// if not enough free MemBlock, subtract n from h.
	if (ring_idx + n > (u32)atomic_read((atomic_t*)&mb_ring->t)){
		atomic_sub(n, h);
		return false;
	}
	// we know these slots (ring_idx to ring_idx+n) in the ring is valid (won't be written by user), because the ring is large enough to contain N_MEM_BLOCK mb
	for (i = 0; i < n; i++){
		u32 mb_idx = mb_ring->v[get_free_mb_ring_idx(ring_idx + i)];
		v[i] = &shmem.addr->mem_block[mb_idx];
	}
	return true;
}

static inline bool init_recorder_mb(struct DeterRecorder* rec){
	u32 i;
	struct MemBlock* mbs[DETER_MEM_BLOCK_TYPE_TOTAL];
	// get a mb for each type of data
	if (!get_n_free_mem_block(DETER_MEM_BLOCK_TYPE_TOTAL, mbs))
		return false;
	// init each mb
	for (i = 0; i < DETER_MEM_BLOCK_TYPE_TOTAL; i++){
		mbs[i]->len = 0;
		mbs[i]->rec_id = rec2idx(rec);
		rec->used_mb++;
	}
	// assign mb to recorder
	rec->evt.mb = mbs[0];		mbs[0]->type = DETER_MEM_BLOCK_TYPE_EVT;
	rec->sockcall.mb = mbs[1];	mbs[1]->type = DETER_MEM_BLOCK_TYPE_SOCKCALL;
	rec->ps.mb = mbs[2];		mbs[2]->type = DETER_MEM_BLOCK_TYPE_PS;
	rec->jif.mb = mbs[3];		mbs[3]->type = DETER_MEM_BLOCK_TYPE_JIF;
	rec->mp.mb = mbs[4];		mbs[4]->type = DETER_MEM_BLOCK_TYPE_MP;
	rec->ma.mb = mbs[5];		mbs[5]->type = DETER_MEM_BLOCK_TYPE_MA;
	rec->ms.mb = mbs[6];		mbs[6]->type = DETER_MEM_BLOCK_TYPE_MS;
	rec->siq.mb = mbs[7];		mbs[7]->type = DETER_MEM_BLOCK_TYPE_SIQ;
	rec->ts.mb = mbs[8];		mbs[8]->type = DETER_MEM_BLOCK_TYPE_TS;
	for (i = 0; i < DETER_EFFECT_BOOL_N_LOC; i++){
		rec->eb[i].mb = mbs[9+i]; mbs[9+i]->type = DETER_MEM_BLOCK_TYPE_EB(i);
	}
	#if ADVANCED_EVENT_ENABLE
	rec->ae.mb = mbs[9 + DETER_EFFECT_BOOL_N_LOC]; mbs[9 + DETER_EFFECT_BOOL_N_LOC]->type = DETER_MEM_BLOCK_TYPE_AE;
	#endif
	return true;
}

/* get a DeterRecorder.
 * Called when a connection is successfully built
 * i.e., after receiving SYN-ACK at client and after receiving ACK at server.
 * So this function is called in bottom-half, so we must not block*/
static void recorder_create(struct sock *sk, struct sk_buff *skb, int mode){
	int i;

	// create DeterRecorder
	struct DeterRecorder *rec = deter_alloc_recorder();
	if (!rec){
		printk("[recorder_create] sport = %hu, dport = %hu, fail to create recorder. h=%u t=%u\n", ntohs(inet_sk(sk)->inet_sport), ntohs(inet_sk(sk)->inet_dport), shmem.addr->free_rec_ring.h, shmem.addr->free_rec_ring.t);
		goto out;
	}
	printk("[recorder_create] sport = %hu, dport = %hu, succeed to create recorder. h=%u t=%u\n", ntohs(inet_sk(sk)->inet_sport), ntohs(inet_sk(sk)->inet_dport), shmem.addr->free_rec_ring.h, shmem.addr->free_rec_ring.t);
	sk->recorder = (void*)rec;

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
	rec->broken = rec->alert = 0;
	rec->used_mb = rec->dump_mb = 0;
	rec->seq = 0;
	atomic_set(&rec->sockcall_id, 0);
	atomic_set(&rec->sockcall_id_mp, 0);
	// get and init MemBlock for each type of data
	while (!init_recorder_mb(rec)); // ensure we get enough valid mb

	// init runtime states
	rec->evt.n = rec->sockcall.n = rec->ps.n = rec->jif.n = rec->mp.n = rec->ma.n = rec->ms.n = rec->siq.n = 0;
	for (i = 0; i < DETER_EFFECT_BOOL_N_LOC; i++)
		rec->eb[i].n = 0;
	rec->ps.last = 0;
	#if COLLECT_TX_STAMP
	rec->ts.n = 0;
	#endif
	#if ADVANCED_EVENT_ENABLE
	rec->ae.n = 0;
	#endif
	// init jif and ma specific runtime state
	rec->jif.last_jiffies = rec->jif.idx_delta = 0;
	rec->ma.last_v = rec->ma.idx_delta = 0;

	if (mode == 0)
		copy_from_server_sock(sk); // copy sock init data
	else 
		copy_from_client_sock(sk);
	rec->mode = mode;
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
		printk("client dip = %08x, sport = %hu, dport = %hu, creating recorder\n", ntohl(dip), ntohs(inet_sk(sk)->inet_sport), ntohs(inet_sk(sk)->inet_dport));
		recorder_create(sk, skb, 1);
	}
}

/* this function should be called in thread_safe environment */
static inline void new_event(struct sock *sk, u32 type){
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
	u32 seq;
	union{
		u64 u;
		struct derand_event e;
	}dt;
	if (!rec)
		return;

	// increment the seq #
	seq = rec->seq++;

	dt.e.seq = seq;
	dt.e.type = type;
	push_u64(rec, &rec->evt.mb, DETER_MEM_BLOCK_TYPE_EVT, dt.u);
	rec->evt.n++;
}

/* destruct a DeterRecorder.
 */
static void recorder_destruct(struct sock *sk){
	struct DeterRecorder* rec = sk->recorder;
	int i;
	if (!rec){
		//printk("[recorder_destruct] sport = %hu, dport = %hu, recorder is NULL.\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		return;
	}

	// add a finish event
	new_event(sk, EVENT_TYPE_FINISH);

	// finish jiffies
	if (rec->jif.idx_delta > 0){
		union {
			u64 u;
			union jiffies_rec jf_rec;
		}dt_jf;
		dt_jf.jf_rec.jiffies_delta = 0;
		dt_jf.jf_rec.idx_delta = rec->jif.idx_delta;
		push_u64(rec, &rec->jif.mb, DETER_MEM_BLOCK_TYPE_JIF, dt_jf.u);
		rec->jif.n++;
	}

	// finish memory_allocated
	if (rec->ma.idx_delta > 0){
		union{
			u64 u;
			union memory_allocated_rec ma_rec;
		}dt_ma;
		dt_ma.ma_rec.v_delta = 0;
		dt_ma.ma_rec.idx_delta = rec->ma.idx_delta;
		push_u64(rec, &rec->ma.mb, DETER_MEM_BLOCK_TYPE_MA, dt_ma.u);
		rec->ma.n++;
	}

	// finish ps
	if (rec->ps.last != 0){
		push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, rec->ps.last);
		rec->ps.n++;
	}

	// put mb to done_mb_ring
	put_done_mem_block(rec->evt.mb);
	put_done_mem_block(rec->sockcall.mb);
	put_done_mem_block(rec->ps.mb);
	put_done_mem_block(rec->jif.mb);
	put_done_mem_block(rec->mp.mb);
	put_done_mem_block(rec->ma.mb);
	put_done_mem_block(rec->ms.mb);
	put_done_mem_block(rec->siq.mb);
	put_done_mem_block(rec->ts.mb);
	for (i = 0; i < DETER_EFFECT_BOOL_N_LOC; i++)
		put_done_mem_block(rec->eb[i].mb);
	#if ADVANCED_EVENT_ENABLE
	put_done_mem_block(rec->ae.mb);
	#endif

	// remove the recorder from sk
	sk->recorder = NULL;
	printk("[recorder_destruct] sport = %hu, dport = %hu, succeed to destruct a recorder.\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
}

/******************************************
 * sockcall
 *****************************************/
static inline void put_sockcall(struct DeterRecorder *rec, int sc_id, struct derand_rec_sockcall* sc){
	if (atomic_read(&rec->sockcall_id_mp) != sc_id){
		u32 cnt = 0;
		for (;atomic_read(&rec->sockcall_id_mp) != sc_id; cnt++){
			if ((cnt & 0x0fffffff) == 0)
				printk("long wait for sockcall_id_mp: %d sc_id %d\n", atomic_read(&rec->sockcall_id_mp), sc_id);
		}
	}
	push_nbyte(rec, &rec->sockcall.mb, DETER_MEM_BLOCK_TYPE_SOCKCALL, sizeof(struct derand_rec_sockcall), sc);
	rec->sockcall.n++;
	atomic_inc(&rec->sockcall_id_mp);
}
static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct DeterRecorder* rec = sk->recorder;
	struct derand_rec_sockcall sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;

	sc.type = DERAND_SOCKCALL_TYPE_SENDMSG;
	sc.sendmsg.flags = msg->msg_flags;
	sc.sendmsg.size = size;
	sc.thread_id = (u64)current;
	put_sockcall(rec, sc_id, &sc);

	// return sockcall ID 
	return sc_id;
}

#if 0
static u32 new_sendpage(struct sock *sk, int offset, size_t size, int flags){
	struct DeterRecorder* rec = sk->recorder;
	if (!rec)
		return 0;
	printk("[DERAND] %hu %hu: tcp_sendpage %d %lu %x!!!\n", ntohs(rec->sport), ntohs(rec->dport), offset, size, flags);
	return 0;
}
#endif

static u32 new_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock, int flags, int *addr_len){
	struct DeterRecorder* rec = sk->recorder;
	struct derand_rec_sockcall sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;

	// store data for this sockcall
	sc.type = DERAND_SOCKCALL_TYPE_RECVMSG;
	sc.recvmsg.flags = nonblock | flags;
	sc.recvmsg.size = len;
	sc.thread_id = (u64)current;
	put_sockcall(rec, sc_id, &sc);

	// return sockcall ID
	return sc_id;
}

static u32 new_splice_read(struct sock *sk, size_t len, unsigned int flags){
	struct DeterRecorder* rec = sk->recorder;
	struct derand_rec_sockcall sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;

	// store data for this sockcall
	sc.type = DERAND_SOCKCALL_TYPE_SPLICE_READ;
	sc.splice_read.flags = flags;
	sc.splice_read.size = len;
	sc.thread_id = (u64)current;
	put_sockcall(rec, sc_id, &sc);

	// return sockcall ID
	return sc_id;
}

static u32 new_close(struct sock *sk, long timeout){
	struct DeterRecorder* rec = sk->recorder;
	struct derand_rec_sockcall sc;
	int sc_id;

	if (!rec)
		return 0;
	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;

	// store data for this sockcall
	sc.type = DERAND_SOCKCALL_TYPE_CLOSE;
	sc.close.timeout = timeout;
	sc.thread_id = (u64)current;
	put_sockcall(rec, sc_id, &sc);

	// return sockcall ID
	return sc_id;
}

static u32 new_setsockopt(struct sock *sk, int level, int optname, char __user *optval, unsigned int optlen){
	struct DeterRecorder* rec = sk->recorder;
	struct derand_rec_sockcall sc;
	int sc_id;

	if (!rec)
		return 0;
	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;

	// store data for this sockcall
	sc.type = DERAND_SOCKCALL_TYPE_SETSOCKOPT;
	if ((u32)level > 255 || (u32)optname > 255 || (u32)optlen > 12){ // use u32 so that negative values are also considered
		sc.setsockopt.level = sc.setsockopt.optname = sc.setsockopt.optlen = 255;
	}else {
		sc.setsockopt.level = level;
		sc.setsockopt.optname = optname;
		sc.setsockopt.optlen = optlen;
		copy_from_user(sc.setsockopt.optval, optval, optlen);
	}
	put_sockcall(rec, sc_id, &sc);

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
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
	if (!rec)
		return;
	rec->seq++;
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
/* get the number of consecutive normal pkt */
static inline u16 ps_get_n_normal(u16 x){
	return x & 0x7fff;
}
/* test if can add more normal to this record */
static inline bool ps_normal_not_full(u16 x){
	return ps_get_n_normal(x) < 0x7fff;
}
void mon_net_action(struct sock *sk, struct sk_buff *skb){
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
	u16 ipid, gap;
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);
	if (!rec)
		return;

	// if this packet ackes our fin, skip. ipid from the other side may not be consecutive after acking our fin
	if (rec->pkt_idx.fin && (ntohl(tcph->ack_seq) == rec->pkt_idx.fin_seq + 1 || ntohl(tcph->ack_seq) == rec->pkt_idx.fin_seq + 2))
		return;
	
	ipid = ntohs(iph->id);

	if (rec->pkt_idx.first){
		// this is the first valid ipid for client side, record and return
		rec->pkt_idx.init_ipid = rec->pkt_idx.last_ipid = ipid;
		rec->pkt_idx.first = 0;
		return;
	}

	gap = get_pkt_idx_gap(&rec->pkt_idx, ipid);
	if (gap == 1){ // normal
		u16 x = rec->ps.last;
		if (ps_normal_not_full(x))
			rec->ps.last = x+1;
		else{
			push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, rec->ps.last);
			rec->ps.n++;
			rec->ps.last = 1;
		}
	}else { // abnormal
		u16 x = rec->ps.last;
		if (x != 0){ // there is some normal stream, push
			push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, x);
			rec->ps.n++;
			rec->ps.last = 0;
		}
		if (gap <= 0x7fff){ // assume forward gap
			if (gap < 0x3fff){ // can be stored in one record
				push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, gap | 0x8000); // 10[gap]
				rec->ps.n++;
			}else{
				push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, 0xffff); // if 0xffff, read the next u16, and use it as the gap
				push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, gap);
				rec->ps.n+=2;
			}
		}else { // assume backward gap
			u16 delta = 65536 - (u32)gap; // -gap
			if (delta < 0x3fff){
				push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, delta | 0xc000); // 11[delta]. At most 0xfffe, because delta < 0x3fff
				rec->ps.n++;
			}else{
				push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, 0xffff); // if 0xffff, read the next u16, and use it as the gap (not delta)
				push_u16(rec, &rec->ps.mb, DETER_MEM_BLOCK_TYPE_PS, gap);
				rec->ps.n+=2;
			}
		}
	}
	update_pkt_idx(&rec->pkt_idx, ipid);
}

void record_fin_seq(struct sock *sk){
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
	if (!rec)
		return;
	rec->pkt_idx.fin_v64 = tcp_sk(sk)->write_seq | (0x100000000);
}

#if ADVANCED_EVENT_ENABLE
/***********************************************
 * Advanced event. Used for debugging.
 **********************************************/
static inline void record_advanced_event(const struct sock *sk, u8 func_num, u8 loc, u8 fmt, int n, ...){
	u32 i, j;
	va_list vl;
	struct DeterRecorder *rec = (struct DeterRecorder*)(sk->recorder);

	// store ae header
	push_u32(rec, &rec->ae.mb, DETER_MEM_BLOCK_TYPE_AE, get_ae_hdr_u32(func_num, loc, n, fmt));
	rec->ae.n++;
	// store data
	va_start(vl, n);
	for (i = 1, j = 1<<(n-1); j; i++, j>>=1){
		if (fmt & j){
			u64 a = va_arg(vl, u64);
			push_u32(rec, &rec->ae.mb, DETER_MEM_BLOCK_TYPE_AE, (u32)(a & 0xffffffff));
			rec->ae.n++;
			i++;
			push_u32(rec, &rec->ae.mb, DETER_MEM_BLOCK_TYPE_AE, (u32)(a >> 32));
			rec->ae.n++;
		}else{
			push_u32(rec, &rec->ae.mb, DETER_MEM_BLOCK_TYPE_AE, va_arg(vl, int));
			rec->ae.n++;
		}
	}
	va_end(vl);
}
#endif

/***********************************************
 * shared variables
 **********************************************/
static void _read_jiffies(const struct sock *sk, unsigned long v, int id){
	struct DeterRecorder* rec = sk->recorder;
	union {
		u64 u;
		union jiffies_rec jf_rec;
	}dt;
	if (!is_valid_recorder(rec))
		return;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -1, id, 0b0, 1, rec->jif.n); // jiffies: type=-1, loc=id, fmt=0, data=jif.n
	#endif

	if (rec->jif.n == 0){
		// push new data
		dt.jf_rec.init_jiffies = v;
		push_u64(rec, &rec->jif.mb, DETER_MEM_BLOCK_TYPE_JIF, dt.u);
		rec->jif.n++;
		// update state
		rec->jif.last_jiffies = v;
		rec->jif.idx_delta = 0;
	}else if (v != rec->jif.last_jiffies){
		// push new data
		dt.jf_rec.jiffies_delta = v - rec->jif.last_jiffies;
		dt.jf_rec.idx_delta = rec->jif.idx_delta + 1;
		push_u64(rec, &rec->jif.mb, DETER_MEM_BLOCK_TYPE_JIF, dt.u);
		rec->jif.n++;
		// update state
		rec->jif.last_jiffies = v;
		rec->jif.idx_delta = 0;
	}else{
		rec->jif.idx_delta++;
	}
}
static void read_jiffies(const struct sock *sk, unsigned long v, int id){
	_read_jiffies(sk, v, id + 100);
}
static void read_tcp_time_stamp(const struct sock *sk, unsigned long v, int id){
	_read_jiffies(sk, v, id);
}

static void record_tcp_under_memory_pressure(const struct sock *sk, bool ret){
	struct DeterRecorder* rec = (struct DeterRecorder*)sk->recorder;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -2, 0, 0b0, 1, (int)ret); // mpq: type=-2, loc=0, fmt=0, data=ret
	#endif
	push_bit(rec, &rec->mp.mb, DETER_MEM_BLOCK_TYPE_MP, (u8)ret);
	rec->mp.n++;
}

static void record_sk_under_memory_pressure(const struct sock *sk, bool ret){
	struct DeterRecorder* rec = (struct DeterRecorder*)sk->recorder;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -2, 0, 0b0, 1, (int)ret); // mpq: type=-2, loc=0, fmt=0, data=ret
	#endif
	push_bit(rec, &rec->mp.mb, DETER_MEM_BLOCK_TYPE_MP, (u8)ret);
	rec->mp.n++;
}

static void record_sk_memory_allocated(const struct sock *sk, long ret){
	struct DeterRecorder* rec = (struct DeterRecorder*)sk->recorder;
	union{
		u64 u;
		union memory_allocated_rec ma_rec;
	}dt;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -3, 0, 0b0, 1, rec->ma.n); // maq: type=-3, loc=0, fmt=0b1, data=ma.n
	#endif

	if (rec->ma.n == 0){
		// push new data
		dt.ma_rec.init_v = ret;
		push_u64(rec, &rec->ma.mb, DETER_MEM_BLOCK_TYPE_MA, dt.u);
		rec->ma.n++;
		// update state
		rec->ma.last_v = ret;
		rec->ma.idx_delta = 0;
	}else if (ret != rec->ma.last_v){
		// push new data
		dt.ma_rec.v_delta = (s32)(ret - rec->ma.last_v);
		dt.ma_rec.idx_delta = rec->ma.idx_delta + 1;
		push_u64(rec, &rec->ma.mb, DETER_MEM_BLOCK_TYPE_MA, dt.u);
		rec->ma.n++;
		// update state
		rec->ma.last_v = ret;
		rec->ma.idx_delta = 0;
	}else{
		rec->ma.idx_delta++;
	}
}

static void record_sk_sockets_allocated_read_positive(struct sock *sk, int ret){
	struct DeterRecorder *rec = sk->recorder;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -4, 0, 0b0, 1, 0); // type=-4, loc=0, fmt=0b00, data=0
	#endif
	rec->n_sockets_allocated++;
}

static void record_skb_mstamp_get(struct sock *sk, struct skb_mstamp *cl, int loc){
	struct DeterRecorder* rec = (struct DeterRecorder*)sk->recorder;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -5, 0, 0b0, 1, rec->ms.n); // msq: type=-5, loc=0, fmt=0b0, data=ms.n
	#endif
	push_u64(rec, &rec->ms.mb, DETER_MEM_BLOCK_TYPE_MS, cl->v64);
	rec->ms.n++;
}

static void record_effect_bool(const struct sock *sk, int loc, bool v){
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
	#if ADVANCED_EVENT_ENABLE
	if (loc != 0) // loc 0 is not serializable among all events, but just within incoming packets
		record_advanced_event(sk, -6, loc, 0b0, 1, rec->eb[loc].n); // ebq: type=-6, loc=loc, fmt=0b0, data=eb[loc].n
	#endif
	push_bit(rec, &rec->eb[loc].mb, DETER_MEM_BLOCK_TYPE_EB(loc), (u8)v);
	rec->eb[loc].n++;
}

static void record_skb_still_in_host_queue(const struct sock *sk, bool ret){
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
	#if ADVANCED_EVENT_ENABLE
	record_advanced_event(sk, -7, 0, 0b0, 1, rec->siq.n); // siqq: type=-7, loc=0, fmt=0b0, data=siqq->t
	#endif
	push_bit(rec, &rec->siq.mb, DETER_MEM_BLOCK_TYPE_SIQ, (u8)ret);
	rec->siq.n++;
}

#if COLLECT_TX_STAMP
static void tx_stamp(const struct sk_buff *skb){
	struct sock* sk = skb->sk;
	struct DeterRecorder* rec;
	u64 clock;

	if (!sk)
		return;
	rec = (struct DeterRecorder*)sk->recorder;
	if (!rec || !is_valid_recorder(rec))
		return;
	clock = local_clock();

	push_u32(rec, &rec->ts.mb, DETER_MEM_BLOCK_TYPE_TS, clock);
	rec->ts.n++;
}
#endif

/******************************
 * alert
 *****************************/
static void record_alert(const struct sock *sk, int loc){
	struct DeterRecorder *rec = (struct DeterRecorder*)sk->recorder;
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
