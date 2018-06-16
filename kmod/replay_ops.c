#include <net/tcp.h>
#include <net/derand_ops.h>
#include "replay_ops.h"
#include "copy_to_sock_init_val.h"

struct replay_ops replay_ops = {
	.replayer = NULL,
	.started = 0,
	.sport = 0x60ea, // ntohs(60000)
	.dport = 0,
	.lock = __SPIN_LOCK_UNLOCKED(),
};

int replay_kthread(void *args){
	printk("replay_kthread started\n");
	return 0;
}

static void server_recorder_create(struct sock *sk){
	uint16_t sport;

	if (!replay_ops.replayer)
		return;

	// return if this is not the socket to replay
	sport = inet_sk(sk)->inet_sport;
	if (sport != replay_ops.sport)
		return;

	// return if the replay has already started
	spin_lock_bh(&replay_ops.lock);
	if (replay_ops.started){
		spin_unlock_bh(&replay_ops.lock);
		return;
	}
	replay_ops.started = 1; // mark started, so other sockets won't get replayed
	spin_unlock_bh(&replay_ops.lock);

	// now, this is the socket to replay
	sk->replayer = replay_ops.replayer;
	replay_ops.dport = inet_sk(sk)->inet_dport;

	// copy sock init data
	copy_to_server_sock(sk);
}

static void recorder_destruct(struct sock *sk){
	// TODO: do some finish job. Print some stats
	printk("replay finish, destruct replayer\n");
}

static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct derand_replayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	return sc_id;
}

static u32 new_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock, int flags, int *addr_len){
	struct derand_replayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	return sc_id;
}

static inline void wait_before_lock(struct sock *sk, u32 type){
	struct derand_replayer *r = (struct derand_replayer*)sk->replayer;
	volatile struct event_q *evtq;
	volatile u32 *seq;
	if (!r)
		return;
	evtq = &r->evtq;
	seq = &r->seq;

	if (evtq->h >= evtq->t){
		printk("Error: more events than recorded, current event type: %u\n", type);
		return;
	}
	// wait until the next event is this event acquiring lock
	while (!(*seq == evtq->v[get_event_q_idx(evtq->h)].seq && evtq->v[get_event_q_idx(evtq->h)].type == type));
}

/**************************************
 * hooks before lock
 *************************************/
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

	// wait until either (1) no more other types of events, or (2) the next event is packet
	while (!(evtq->h >= evtq->t || *seq < evtq->v[get_event_q_idx(evtq->h)].seq));
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
		printk("more jiffies reads then recorded\n");
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
		printk("more memory_allocated reads than recorded\n");
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
		printk("more skb_mstamp_get than recorded\n");
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
		printk("more effect_bool %d than recorded\n", loc);
		idx = get_eb_q_idx(ebq->t - 1);
	}else{
		idx = get_eb_q_idx(ebq->h);
		ebq->h++;
	}
	bit_idx = idx & 31;
	arr_idx = idx / 32;
	return (ebq->v[arr_idx] >> bit_idx) & 1;
}

void bind_replay_ops(void){
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
}

void unbind_replay_ops(void){
	derand_replay_effect_bool = NULL;
	derand_record_ops = derand_record_ops_default;
}
