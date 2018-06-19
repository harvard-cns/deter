#include <linux/kthread.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/tcp.h>
#include <net/derand_ops.h>
#include "replay_ops.h"
#include "copy_to_sock_init_val.h"
#include "logger.h"

void derand_tcp_tasklet_func(struct sock *sk);

struct replay_ops replay_ops = {
	.replayer = NULL,
	.state = NOT_STARTED,
	.sip = 0,
	.dip = 0,
	.sport = 0x60ea, // ntohs(60000)
	.dport = 0,
	.lock = __SPIN_LOCK_UNLOCKED(),
};

static struct task_struct *replay_task = NULL;

/*******************************************************
 * function for logging derand_replayer stats
 ******************************************************/
void logging_stats(struct derand_replayer *r){
	int i;
	derand_log("replay finish, destruct replayer\n");
	derand_log("sockcall_id: %d\n", atomic_read(&r->sockcall_id));
	derand_log("seq: %u\n", r->seq);
	derand_log("evtq: h=%u t=%u\n", r->evtq.h, r->evtq.t);
	derand_log("jfq: h=%u t=%u\n", r->jfq.h, r->jfq.t);
	derand_log("mpq: h=%u t=%u\n", r->mpq.h, r->mpq.t);
	derand_log("maq: h=%u t=%u\n", r->maq.h, r->maq.t);
	derand_log("msq: h=%u t=%u\n", r->msq.h, r->msq.t);
	for (i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		derand_log("ebq[%d]: h=%u t=%u\n", i, r->ebq[i].h, r->ebq[i].t);
}

/********************************************************
 * The queue between replay kthread and packet corrector
 ********************************************************/
#define PACKET_QUEUE_SIZE 8192
struct Packet{
	struct sk_buff *skb;
	struct sock *sk;
	struct net *net;
	int (*okfn)(struct net *, struct sock *, struct sk_buff *);
};
struct PacketQueue{
	u32 h, t;
	struct Packet q[PACKET_QUEUE_SIZE];
};
#define get_pkt_q_idx(i) ((i) & (PACKET_QUEUE_SIZE - 1))

// the queue
struct PacketQueue pkt_q;

// enqueue
static inline void enqueue(struct PacketQueue *q, struct sk_buff *skb, const struct nf_hook_state *state){
	if (unlikely(q->t == q->h + PACKET_QUEUE_SIZE)){
		derand_log("WARNING: PacketQueue full!\n");
		return;
	}
	q->q[get_pkt_q_idx(q->t)] = (struct Packet){.skb = skb, .sk = state->sk, .net = state->net, .okfn = state->okfn};
	q->t++;
}

// dequeue
static int dequeue(struct PacketQueue *q){
	struct Packet *p;
	if (q->h >= q->t)
		return -1;
	p = &q->q[get_pkt_q_idx(q->h)];

	// because we are replaying bh events, we want to disable bh & preempt
	preempt_disable();
	local_bh_disable();
	p->okfn(p->net, p->sk, p->skb);
	local_bh_enable();
	preempt_enable();

	q->h++;

	return 0;
}

/******************************************************
 * The kthread of replay
 *****************************************************/
int replay_kthread(void *args){
	int i, n_pkt_btw;
	struct event_q *evtq = &replay_ops.replayer->evtq;
	derand_log("replay_kthread started\n");

	while (replay_ops.state == NOT_STARTED);
	if (replay_ops.state >= SHOULD_STOP)
		goto end;

	/* Issue all kernel events in order.
	 * Note that we need to counter the number of packet events (n_pkt_btw) between two other events:
	 * it should be the gap of seq # between two non-pkt kernel events (timer or tasklet), minus #sockcall events
	 * example: timer seq 2 (last_seq), sockcall seq 4, tasklet seq 7 (current_seq)
	 *          after timer seq 2, we should have i = 7,
	 *                                and n_pkt_btw = current_seq-(last_seq+1)-1 = 7-(2+1)-1;
	 * */
	for (i = 0, n_pkt_btw = 0; i < evtq->t && replay_ops.state == STARTED; i++){
		// skip sockcall
		if (evtq->v[i].type >= DERAND_SOCK_ID_BASE){
			n_pkt_btw--; // n_pkt_btw-- for every sockcall
			continue;
		}
		n_pkt_btw += evtq->v[get_event_q_idx(i)].seq; // n_pkt_btw += current_seq
		derand_log("[kthread] next: %d pkts, evtq->v[%d(%d)]={seq:%u, type:%u}\n", n_pkt_btw, get_event_q_idx(i), i, evtq->v[get_event_q_idx(i)].seq, evtq->v[get_event_q_idx(i)].type);

		// send packets in between
		for (; n_pkt_btw && replay_ops.state == STARTED; n_pkt_btw--){
			while (replay_ops.state == STARTED && dequeue(&pkt_q));
			derand_log("[kthread] sent a packet, %d remains\n", n_pkt_btw);
		}

		preempt_disable();
		local_bh_disable();
		derand_log("[kthread] BEFORE issuing the event of type %u\n", evtq->v[get_event_q_idx(i)].type);
		// issue this event
		switch (evtq->v[get_event_q_idx(i)].type){
			case EVENT_TYPE_TASKLET:
				derand_tcp_tasklet_func(replay_ops.sk);
				break;
			case EVENT_TYPE_WRITE_TIMEOUT:
				replay_ops.write_timer_func((unsigned long)replay_ops.sk);
				break;
			case EVENT_TYPE_DELACK_TIMEOUT:
				replay_ops.delack_timer_func((unsigned long)replay_ops.sk);
				break;
			case EVENT_TYPE_KEEPALIVE_TIMEOUT:
				replay_ops.keepalive_timer_func((unsigned long)replay_ops.sk);
				break;
			case EVENT_TYPE_FINISH:
				break;
			default:
				derand_log("[kthread] WARNING: unknown event type: %d\n", evtq->v[get_event_q_idx(i)].type);
		}
		derand_log("[kthread] AFTER issuing the event of type %u\n", evtq->v[get_event_q_idx(i)].type);
		local_bh_enable();
		preempt_enable();

		// update n_pkt_btw
		n_pkt_btw = -(int)evtq->v[get_event_q_idx(i)].seq - 1; // n_pkt_btw = -(last_seq+1)
	}

end:
	derand_log("[kthread] kthread finishes\n");
	replay_ops.state = STOPPED;
	return 0;
}

/**********************************************
 * Null timer handler
 *********************************************/
static void null_timer(unsigned long data){
}

static void set_null_timer(struct sock *sk){
	struct inet_connection_sock *icsk = inet_csk(sk);

	replay_ops.write_timer_func = icsk->icsk_retransmit_timer.function;
	icsk->icsk_retransmit_timer.function = null_timer;

	replay_ops.delack_timer_func = icsk->icsk_delack_timer.function;
	icsk->icsk_delack_timer.function = null_timer;

	replay_ops.keepalive_timer_func = sk->sk_timer.function;
	sk->sk_timer.function = null_timer;
}

/*********************************************
 * hooks for replayer create/destruct
 ********************************************/
static void server_recorder_create(struct sock *sk){
	uint16_t sport;

	if (!replay_ops.replayer)
		return;

	// return if this is not the socket to replay
	sport = inet_sk(sk)->inet_sport;
	if (sport != replay_ops.sport)
		return;

	derand_log("a new server sock created\n");

	// return if the replay has already started
	spin_lock_bh(&replay_ops.lock);
	if (replay_ops.state != NOT_STARTED){
		spin_unlock_bh(&replay_ops.lock);
		return;
	}

	// now, this is the socket to replay
	sk->replayer = replay_ops.replayer;
	
	// record 4 tuples
	replay_ops.dport = inet_sk(sk)->inet_dport;
	replay_ops.sip = inet_sk(sk)->inet_saddr;
	replay_ops.dip = inet_sk(sk)->inet_daddr;

	// record sk
	replay_ops.sk = sk;

	// copy sock init data
	derand_log("copy init variables to the sock\n");
	copy_to_server_sock(sk);

	// set timer handlers to null function
	derand_log("record timer; set timer to Null\n");
	set_null_timer(sk);

	// mark started, so other sockets won't get replayed
	replay_ops.state = STARTED;
	spin_unlock_bh(&replay_ops.lock);
}

static void recorder_destruct(struct sock *sk){
	// do some finish job. Print some stats
	struct derand_replayer *r = sk->replayer;
	if (!r)
		return;
	replay_ops.state = SHOULD_STOP;
}

/****************************************
 * hooks for assigning sockcall id
 ***************************************/
static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct derand_replayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	derand_log("a new tcp_sendmsg, assign id %d\n", sc_id);
	return sc_id;
}

static u32 new_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock, int flags, int *addr_len){
	struct derand_replayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	derand_log("a new tcp_recvmsg, assign id %d\n", sc_id);
	return sc_id;
}

/**************************************
 * hooks before lock
 *************************************/
static inline void wait_before_lock(struct sock *sk, u32 type){
	struct derand_replayer *r = (struct derand_replayer*)sk->replayer;
	volatile struct event_q *evtq;
	volatile u32 *seq;
	if (!r)
		return;
	evtq = &r->evtq;
	seq = &r->seq;

	derand_log("event type %u is going to acquire lock\n", type);
	if (evtq->h >= evtq->t){
		derand_log("Error: more events than recorded, current event type: %u\n", type);
		return;
	}
	// wait until the next event is this event acquiring lock
	while (replay_ops.state == STARTED && !(*seq == evtq->v[get_event_q_idx(evtq->h)].seq && evtq->v[get_event_q_idx(evtq->h)].type == type));
}

static void sockcall_before_lock(struct sock *sk, u32 sc_id){
	wait_before_lock(sk, sc_id + DERAND_SOCK_ID_BASE);
}

static void incoming_pkt_before_lock(struct sock *sk){
	struct derand_replayer *r = (struct derand_replayer*)sk->replayer;
	volatile struct event_q *evtq;
	volatile u32 *seq;
	if (!r)
		return;
	evtq = &r->evtq;
	seq = &r->seq;

	derand_log("incoming pkt is going to acquire lock\n");
	// wait until either (1) no more other types of events, or (2) the next event is packet
	while (replay_ops.state == STARTED && !(evtq->h >= evtq->t || *seq < evtq->v[get_event_q_idx(evtq->h)].seq));
}

static void write_timer_before_lock(struct sock *sk){
	wait_before_lock(sk, EVENT_TYPE_WRITE_TIMEOUT);
}

static void delack_timer_before_lock(struct sock* sk){
	wait_before_lock(sk, EVENT_TYPE_DELACK_TIMEOUT);
}

static void keepalive_timer_before_lock(struct sock *sk){
	wait_before_lock(sk, EVENT_TYPE_KEEPALIVE_TIMEOUT);
}

static void tasklet_before_lock(struct sock *sk){
	wait_before_lock(sk, EVENT_TYPE_TASKLET);
}

/**************************************
 * hooks after lock
 *************************************/
static inline void new_event(struct derand_replayer *r, u32 type){
	if (!r)
		return;

	derand_log("event type %u acquires the lock!\n", type);
	if (type != EVENT_TYPE_PACKET)
		r->evtq.h++;
	r->seq++;
}

static void sockcall_lock(struct sock *sk, u32 sc_id){
	new_event(sk->replayer, sc_id + DERAND_SOCK_ID_BASE);
}

static void incoming_pkt(struct sock *sk){
	new_event(sk->replayer, EVENT_TYPE_PACKET);
}

static void write_timer(struct sock *sk){
	new_event(sk->replayer, EVENT_TYPE_WRITE_TIMEOUT);
}

static void delack_timer(struct sock *sk){
	new_event(sk->replayer, EVENT_TYPE_DELACK_TIMEOUT);
}

static void keepalive_timer(struct sock *sk){
	new_event(sk->replayer, EVENT_TYPE_KEEPALIVE_TIMEOUT);
}

static void tasklet(struct sock *sk){
	new_event(sk->replayer, EVENT_TYPE_TASKLET);
}

/************************************
 * read values
 ***********************************/
static unsigned long replay_jiffies(const struct sock *sk, int id){
	struct jiffies_q *jfq = &((struct derand_replayer*)sk->replayer)->jfq;
	if (jfq->h >= jfq->t){
		derand_log("more jiffies reads then recorded\n");
		return jfq->last_jiffies;
	}
	if (jfq->h == 0){
		jfq->last_jiffies = jfq->v[0].init_jiffies;
		jfq->h = 1;
		jfq->idx_delta = 0;
	}else {
		jfq->idx_delta++;
		if (jfq->idx_delta == jfq->v[get_jiffies_q_idx(jfq->h)].idx_delta){
			jfq->last_jiffies += jfq->v[get_jiffies_q_idx(jfq->h)].jiffies_delta;
			jfq->idx_delta = 0;
			jfq->h++;
		}
	}
	return jfq->last_jiffies;
}

static bool replay_tcp_under_memory_pressure(const struct sock *sk){
	struct memory_pressure_q *mpq = &((struct derand_replayer*)sk->replayer)->mpq;
	if (mpq->h < mpq->t && mpq->v[get_mp_q_idx(mpq->h)] == mpq->seq){
		mpq->seq++;
		mpq->h++;
		return true;
	}
	mpq->seq++;
	return false;
}

static long replay_sk_memory_allocated(const struct sock *sk){
	struct memory_allocated_q *maq = &((struct derand_replayer*)sk->replayer)->maq;
	if (maq->h >= maq->t){
		derand_log("more memory_allocated reads than recorded\n");
		return maq->last_v;
	}
	if (maq->h == 0){
		maq->last_v = maq->v[0].init_v;
		maq->h = 1;
		maq->idx_delta = 0;
	}else {
		maq->idx_delta++;
		if (maq->idx_delta == maq->v[get_ma_q_idx(maq->h)].idx_delta){
			maq->last_v += maq->v[get_ma_q_idx(maq->h)].v_delta;
			maq->idx_delta = 0;
			maq->h++;
		}
	}
	return maq->last_v;
}

static void replay_skb_mstamp_get(struct sock *sk, struct skb_mstamp *cl, int loc){
	struct mstamp_q *msq = &((struct derand_replayer*)sk->replayer)->msq;
	if (msq->h >= msq->t){
		derand_log("more skb_mstamp_get than recorded\n");
		*cl = msq->v[get_mstamp_q_idx(msq->t - 1)];
		return;
	}
	*cl = msq->v[get_mstamp_q_idx(msq->h)];
	msq->h++;
}

static bool replay_effect_bool(const struct sock *sk, int loc){
	struct effect_bool_q *ebq = &((struct derand_replayer*)sk->replayer)->ebq[loc];
	u32 idx, bit_idx, arr_idx;
	if (ebq->h >= ebq->t){
		derand_log("more effect_bool %d than recorded\n", loc);
		idx = get_eb_q_idx(ebq->t - 1);
	}else{
		idx = get_eb_q_idx(ebq->h);
		ebq->h++;
	}
	bit_idx = idx & 31;
	arr_idx = idx / 32;
	return (ebq->v[arr_idx] >> bit_idx) & 1;
}

/****************************************
 * Packet corrector
 ***************************************/
static inline bool should_replay_skb(struct sk_buff *skb){
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);
	return replay_ops.state == STARTED &&
		   iph->saddr == replay_ops.dip &&
		   iph->daddr == replay_ops.sip &&
		   tcph->source == replay_ops.dport &&
		   tcph->dest == replay_ops.sport;
}

static unsigned int packet_corrector_fn(void *priv, struct sk_buff *skb, const struct nf_hook_state *state){
	// if this packet should not be replayed, skip
	if (!should_replay_skb(skb))
		return NF_ACCEPT;

	{
		struct iphdr *iph = ip_hdr(skb);
		struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);
		derand_log("received pkt: ipid: %hu seq: %hu ack: %hu\n", ntohs(iph->id), ntohl(tcph->seq), ntohl(tcph->ack_seq));
	}
	// TODO: check if we should drop or mark CE bit

	// enqueue this packet
	enqueue(&pkt_q, skb, state);
	return NF_STOLEN;
}

static struct nf_hook_ops pc_nf_hook_ops = {
	.hook = packet_corrector_fn,
	.pf = NFPROTO_IPV4,
	.hooknum = NF_INET_LOCAL_IN,
	.priority = NF_IP_PRI_FIRST,
};

int setup_packet_corrector(void){
	pkt_q.h = pkt_q.t = 0;
	if (nf_register_hook(&pc_nf_hook_ops)){
		derand_log("Cannot register packet corrector's netfilter hook\n");
		return -1;
	}
	return 0;
}
void stop_packet_corrector(void){
	nf_unregister_hook(&pc_nf_hook_ops);
}

/***************************************
 * replay ops setup
 **************************************/
int bind_replay_ops(void){
	if (setup_packet_corrector())
		return -1;

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

	derand_record_ops.sockcall_before_lock = sockcall_before_lock;
	derand_record_ops.incoming_pkt_before_lock = incoming_pkt_before_lock;
	derand_record_ops.write_timer_before_lock = write_timer_before_lock;
	derand_record_ops.delack_timer_before_lock = delack_timer_before_lock;
	derand_record_ops.keepalive_timer_before_lock = keepalive_timer_before_lock;
	derand_record_ops.tasklet_before_lock = tasklet_before_lock;

	derand_record_ops.replay_jiffies = replay_jiffies;
	derand_record_ops.replay_tcp_time_stamp = replay_jiffies;
	derand_record_ops.replay_tcp_under_memory_pressure = replay_tcp_under_memory_pressure;
	derand_record_ops.replay_sk_under_memory_pressure = replay_tcp_under_memory_pressure;
	derand_record_ops.replay_sk_memory_allocated = replay_sk_memory_allocated;
	derand_record_ops.replay_sk_socket_allocated_read_positive = NULL; // TODO: to add
	derand_record_ops.skb_mstamp_get = replay_skb_mstamp_get;
	derand_replay_effect_bool = replay_effect_bool;

	/* The recorder_create functions must be bind last, because they are the enabler of record */
	derand_record_ops.server_recorder_create = server_recorder_create;
	return 0;
}

void unbind_replay_ops(void){
	stop_packet_corrector();
	derand_replay_effect_bool = NULL;
	derand_record_ops = derand_record_ops_default;
}

static void initialize_replay(struct derand_replayer *r){
	int i;

	// init sockcall_id
	r->seq = 0;
	atomic_set(&r->sockcall_id, 0);

	// initialize evtq
	r->evtq.h = 0;

	// initialize jfq
	r->jfq.h = 0;
	if (r->jfq.t > 0)
		r->jfq.last_jiffies = r->jfq.v[0].init_jiffies;
	r->jfq.idx_delta = 0;

	// initialize mpq
	r->mpq.h = 0;
	r->mpq.seq = 0;

	// initialize maq
	r->maq.h = 0;
	if (r->maq.t > 0)
		r->maq.last_v = r->maq.v[0].init_v;
	r->maq.idx_delta = 0;

	// initialize msq
	r->msq.h= 0;

	// initialize ebq
	for (i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		r->ebq[i].h = 0;

	//derand_log("%u %u %u %u %u\n", r->evtq.t, r->jfq.t, r->mpq.t, r->maq.t, r->msq.t);
}

int replay_ops_start(struct derand_replayer *addr){
	replay_ops.replayer = addr;

	// initialize data
	initialize_replay(replay_ops.replayer);

	// bind ops
	if (bind_replay_ops())
		return -1;

	derand_log("create kthread\n");
	// start the kthread
	replay_task = kthread_run(replay_kthread, NULL, "replay_kthread");

	return 0;
}

void replay_ops_stop(void){
	derand_log("unbind_replay_ops\n");
	unbind_replay_ops();

	if (replay_ops.state == STARTED)
		replay_ops.state = SHOULD_STOP;

	// wait for stop kthread to stop
	derand_log("wait for kthread to stop\n");
	while (replay_ops.state != STOPPED);

	// print some stats
	logging_stats(replay_ops.replayer);

	// clean remaining packets in the queue
	while (!dequeue(&pkt_q))
		derand_log("sent a leftover pkt\n");
}
