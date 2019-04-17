#include "utils.h"
#include <linux/kthread.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/tcp.h>
#include <linux/skbuff.h>
#include <net/deter_ops.h>
#include "replay_ops.h"
#include "copy_to_sock_init_val.h"
#include "logger.h"

void deter_tcp_tasklet_func(struct sock *sk);

struct replay_ops replay_ops = {
	.replayer = NULL,
	.state = NOT_STARTED,
	.sip = 0,
	.dip = 0,
	.sport = 0,
	.dport = 0,
	.lock = __SPIN_LOCK_UNLOCKED(),
};

static struct task_struct *replay_task = NULL;

/*******************************************************
 * function for logging DeterReplayer stats
 ******************************************************/
void logging_stats(struct DeterReplayer *r){
	int i;
	deter_log("replay finish, destruct replayer\n");
	deter_log("sockcall_id: %d\n", atomic_read(&r->sockcall_id));
	deter_log("seq: %u\n", r->seq);
	deter_log("evtq: h=%u t=%u\n", r->evtq.h, r->evtq.t);
	deter_log("jfq: h=%u t=%u\n", r->jfq.h, r->jfq.t);
	deter_log("mpq: h=%u t=%u\n", r->mpq.h, r->mpq.t);
	deter_log("maq: h=%u t=%u\n", r->maq.h, r->maq.t);
	deter_log("msq: h=%u t=%u\n", r->msq.h, r->msq.t);
	for (i = 0; i < DETER_EFFECT_BOOL_N_LOC; i++)
		deter_log("ebq[%d]: h=%u t=%u\n", i, r->ebq[i].h, r->ebq[i].t);
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
	if (unlikely(q->t >= q->h + PACKET_QUEUE_SIZE)){
		deter_log("WARNING: PacketQueue full!\n");
		return;
	}
	rmb();
	q->q[get_pkt_q_idx(q->t)] = (struct Packet){.skb = skb, .sk = state->sk, .net = state->net, .okfn = state->okfn};
	wmb();
	q->t++;
}

// dequeue
static int dequeue(struct PacketQueue *q){
	struct Packet *p;
	if (q->h >= q->t)
		return -1;
	rmb();
	p = &q->q[get_pkt_q_idx(q->h)];
	{
		struct iphdr *iph = ip_hdr(p->skb);
		u16 ipid = ntohs(iph->id);
		deter_log("dequeue pkt ipid=%hu\n", ipid);
	}

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
	u32 cnt = 0;
	deter_log("replay_kthread started\n");

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
		if (evtq->v[get_event_q_idx(i)].type >= DETER_SOCK_ID_BASE){
			n_pkt_btw--; // n_pkt_btw-- for every sockcall
			continue;
		}
		n_pkt_btw += evtq->v[get_event_q_idx(i)].seq; // n_pkt_btw += current_seq
		deter_log("[kthread] next: %d pkts, evtq->v[%d(%d)]={seq:%u, type:%u}\n", n_pkt_btw, get_event_q_idx(i), i, evtq->v[get_event_q_idx(i)].seq, evtq->v[get_event_q_idx(i)].type);

		// send packets in between
		for (; n_pkt_btw && replay_ops.state == STARTED; n_pkt_btw--){
			cnt = 0;
			while (replay_ops.state == STARTED && dequeue(&pkt_q)){
				cnt++;
				#if DETER_DEBUG
				if (!(cnt & 0x0fffffff))
					deter_log("[kthread] long wait for pkt: irqs_disabled:%u in_interrupt:%x local_softirq_pending:%x\n", irqs_disabled(), in_interrupt(), local_softirq_pending());
				#endif
			}
			deter_log("[kthread] sent a packet, %d remains\n", n_pkt_btw - 1);
		}

		preempt_disable();
		local_bh_disable();
		deter_log("[kthread] BEFORE issuing the event of type %u\n", evtq->v[get_event_q_idx(i)].type);
		// issue this event
		switch (evtq->v[get_event_q_idx(i)].type){
			case EVENT_TYPE_TASKLET:
				deter_tcp_tasklet_func(replay_ops.sk);
				break;
			case EVENT_TYPE_WRITE_TIMEOUT:
				// We must deactivate the timers, because it affects sk->refcnt. The timer function calls sk_reset_timer(), which inc refcnt only if the timer is inactive/expired. Thus, if we do not deactivate the timer here, refcnt will not inc. Same for other timers.
				del_timer(&inet_csk(replay_ops.sk)->icsk_retransmit_timer);
				replay_ops.write_timer_func((unsigned long)replay_ops.sk);
				break;
			case EVENT_TYPE_DELACK_TIMEOUT:
				del_timer(&inet_csk(replay_ops.sk)->icsk_delack_timer);
				replay_ops.delack_timer_func((unsigned long)replay_ops.sk);
				break;
			case EVENT_TYPE_KEEPALIVE_TIMEOUT:
				del_timer(&replay_ops.sk->sk_timer);
				replay_ops.keepalive_timer_func((unsigned long)replay_ops.sk);
				break;
			case EVENT_TYPE_FINISH:
				break;
			default:
				deter_log("[kthread] WARNING: unknown event type: %d\n", evtq->v[get_event_q_idx(i)].type);
		}
		deter_log("[kthread] AFTER issuing the event of type %u\n", evtq->v[get_event_q_idx(i)].type);
		local_bh_enable();
		preempt_enable();

		// update n_pkt_btw
		n_pkt_btw = -(int)evtq->v[get_event_q_idx(i)].seq - 1; // n_pkt_btw = -(last_seq+1)
	}

end:
	deter_log("[kthread] kthread finishes\n");
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
static inline void replayer_create(struct sock *sk, struct sk_buff *skb){
	struct DeterReplayer *rep;

	// return if the replay has already started
	spin_lock_bh(&replay_ops.lock);
	if (replay_ops.state != NOT_STARTED){
		spin_unlock_bh(&replay_ops.lock);
		return;
	}

	// now, this is the socket to replay
	rep = sk->replayer = replay_ops.replayer;
	
	// init PktIdx
	if (rep->mode == 0){ // ipid is consecutive at this point only for server side
		rep->pkt_idx.init_ipid = rep->pkt_idx.last_ipid = ntohs(ip_hdr(skb)->id);
		rep->pkt_idx.first = 0;
	}else { // in our custom kernel, the first data/ack packet has ipid 1
		rep->pkt_idx.init_ipid = rep->pkt_idx.last_ipid = 0; // we set last_ipid to 0, so the next data/ack pkt should just be 0 +1 = 1
		rep->pkt_idx.first = 0;
		/*
		// for client side, the next packet carries the first valid ipid
		rep->pkt_idx.first = 1;
		*/
	}
	rep->pkt_idx.idx = 0;
	rep->pkt_idx.fin_seq = 0;
	rep->pkt_idx.fin = 0;

	// record 4 tuples
	replay_ops.sport = inet_sk(sk)->inet_sport;
	replay_ops.dport = inet_sk(sk)->inet_dport;
	replay_ops.sip = inet_sk(sk)->inet_saddr;
	replay_ops.dip = inet_sk(sk)->inet_daddr;

	// record sk
	replay_ops.sk = sk;

	// copy sock init data
	deter_log("copy init variables to the sock\n");
	copy_to_sock(sk);

	// set timer handlers to null function
	deter_log("record timer; set timer to Null\n");
	set_null_timer(sk);

	// mark started, so other sockets won't get replayed
	replay_ops.state = STARTED;
	spin_unlock_bh(&replay_ops.lock);
}

static int to_replay_server(struct sock *sk){
	uint16_t sport;

	if (!replay_ops.replayer)
		return false;

	if (replay_ops.replayer->mode != 0)
		return false;

	// return false if this is not the socket to replay
	sport = inet_sk(sk)->inet_sport;
	return sport == replay_ops.replayer->port;
}

static void server_recorder_create(struct sock *sk, struct sk_buff *skb){
	if (!to_replay_server(sk))
		return;

	deter_log("a new server sock created\n");

	replayer_create(sk, skb);
}

static void client_recorder_create(struct sock *sk, struct sk_buff *skb){
	uint16_t dport;

	if (!replay_ops.replayer)
		return;

	if (replay_ops.replayer->mode != 1)
		return;

	// return if this is not the socket to replay
	dport = inet_sk(sk)->inet_dport;
	if (dport != replay_ops.replayer->port)
		return;

	deter_log("a new client sock created\n");

	replayer_create(sk, skb);
}

static void finish_sock(struct sock *sk);
static void recorder_destruct(struct sock *sk){
	// do some finish job. Print some stats
	struct DeterReplayer *r = sk->replayer;
	if (!r)
		return;
	finish_sock(sk);
	deter_log("recorder_destruct: h:%u t:%u\n", r->evtq.h, r->evtq.t);
	if (replay_ops.state == STARTED)
		replay_ops.state = SHOULD_STOP;
}

/****************************************
 * hooks for assigning sockcall id
 ***************************************/
static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct DeterReplayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	deter_log("a new tcp_sendmsg, assign id %d\n", sc_id);
	return sc_id;
}

static u32 new_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock, int flags, int *addr_len){
	struct DeterReplayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	deter_log("a new tcp_recvmsg, assign id %d\n", sc_id);
	return sc_id;
}

static u32 new_close(struct sock *sk, long timeout){
	struct DeterReplayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	deter_log("a new tcp_close, assign id %d\n", sc_id);
	return sc_id;
}

static u32 new_setsockopt(struct sock *sk, int level, int optname, char __user *optval, unsigned int optlen){
	struct DeterReplayer *r = sk->replayer;
	int sc_id;
	if (!r)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &r->sockcall_id) - 1;
	deter_log("a new setsockopt, assign id %d\n", sc_id);
	return sc_id;
}

/**************************************
 * hooks before lock
 *************************************/
static inline void wait_before_lock(struct sock *sk, u32 type){
	struct DeterReplayer *r = (struct DeterReplayer*)sk->replayer;
	struct event_q *evtq;
	u32 *seq;
	u32 sc_id, loc;
	u32 evtq_h;
	#if DETER_DEBUG
	u32 cnt = 0;
	#endif
	if (!r)
		return;
	evtq = &r->evtq;
	seq = &r->seq;

	if (type >= DETER_SOCK_ID_BASE){
		sc_id = (type - DETER_SOCK_ID_BASE) & 0x0fffffff;
		loc = (type - DETER_SOCK_ID_BASE) >> 28;
		deter_log("Before lock: sockcall %u (%u %u); irqs_disabled:%u in_interrupt:%x local_softirq_pending:%x\n", sc_id, loc >> 1, loc & 1, irqs_disabled(), in_interrupt(), local_softirq_pending());
	}else 
		deter_log("Before lock: event type %u; irqs_disabled:%u in_interrupt:%x local_softirq_pending:%x\n", type, irqs_disabled(), in_interrupt(), local_softirq_pending());

	if (atomic_read_u32(&evtq->h) >= evtq->t){
		deter_log("Error: more events than recorded\n");
		return;
	}
	// wait until the next event is this event acquiring lock
	// Note: Be careful about the potential concurrency bugs in the for condition.
	//       There are 2 comparisons: seq == evtq->v[evtq->h].seq, and evtq->v[evtq->h].type == type.
	//       We must first read out the evtq->h (into evtq_h), then use this value for both comparision.
	//       Otherwise, it is possible that in the first comparison, we read the old evtq->h, then in the 
	//       second comparison, we read the new evtq->h.
	for (evtq_h = atomic_read_u32(&evtq->h); replay_ops.state == STARTED && !(atomic_read_u32(seq) == evtq->v[get_event_q_idx(evtq_h)].seq && evtq->v[get_event_q_idx(evtq_h)].type == type); evtq_h = atomic_read_u32(&evtq->h)){
		// if this is in __release_sock, bh is disabled. We cannot busy wait here because it prevents softirq accepting new packets (NET_RX), which we are waiting for, causing deadlock!
		if (type >= DETER_SOCK_ID_BASE && in_softirq())
			cond_resched_softirq();
		#if DETER_DEBUG
		{
			u32 evtq_type = evtq->v[get_event_q_idx(atomic_read_u32(&evtq->h))].type;
			if (atomic_read_u32(seq) == evtq->v[get_event_q_idx(atomic_read_u32(&evtq->h))].seq && evtq_type >= DETER_SOCK_ID_BASE && type >= DETER_SOCK_ID_BASE && evtq_type == type){
				static int first = 0;
				if (first < 128){
					u32 evtq_sc_id = (evtq_type - DETER_SOCK_ID_BASE) & 0x0fffffff;
					u32 evtq_loc = (evtq_type - DETER_SOCK_ID_BASE) >> 28;
					first++;
					if (first > 8)
						deter_log("Error: current sockcall %u (%u %u) != expected sockcall $%u (%u %u)\n", sc_id, loc >> 1, loc & 1, evtq_sc_id, evtq_loc >> 1, evtq_loc & 1);
				}
			}
		}
		#endif
		#if DETER_DEBUG
		if (!(cnt & 0xfffffff)){
			volatile u32 *q_h = &pkt_q.h, *q_t = &pkt_q.t;
			deter_log("long wait [%u %u]: pkt_q: %u %u; irqs_disabled:%u in_interrupt:%x local_softirq_pending:%x\n", atomic_read_u32(seq), evtq->v[get_event_q_idx(atomic_read_u32(&evtq->h))].seq, *q_h, *q_t, irqs_disabled(), in_interrupt(), local_softirq_pending());
		}
		cnt++;
		#endif
	}

	if (type >= DETER_SOCK_ID_BASE){
		u32 sc_id = (type - DETER_SOCK_ID_BASE) & 0x0fffffff;
		u32 loc = (type - DETER_SOCK_ID_BASE) >> 28;
		deter_log("finish wait lock: seq (%u %u) sockcall %u (%u %u)\n", atomic_read_u32(seq), evtq->v[get_event_q_idx(atomic_read_u32(&evtq->h))].seq, sc_id, loc >> 1, loc & 1);
	}else 
		deter_log("finish wait lock: seq (%u %u) event type %u\n", atomic_read_u32(seq), evtq->v[get_event_q_idx(atomic_read_u32(&evtq->h))].seq, type);
}

static void sockcall_before_lock(struct sock *sk, u32 sc_id){
	wait_before_lock(sk, sc_id + DETER_SOCK_ID_BASE);
}

static void incoming_pkt_before_lock(struct sock *sk){
	struct DeterReplayer *r = (struct DeterReplayer*)sk->replayer;
	struct event_q *evtq;
	u32 *seq;
	u32 seq_v, evtq_h_seq_v;
	u32 cnt;
	if (!r)
		return;
	evtq = &r->evtq;
	seq = &r->seq;

	deter_log("Before lock: incoming pkt\n");
	while (replay_ops.state == STARTED){
		// wait until either (1) no more other types of events, or (2) the next event is packet
		// NOTE: evtq->v[get_event_q_idx(evtq->h)].seq must be before *seq
		//       because if first read seq, then h, we might have read the old seq, then the new h, which points to a new place in v, thus giving v[h].seq > seq
		//       but if first h then seq, the value of seq must be newer than h, so there won't be such bug
		evtq_h_seq_v = evtq->v[get_event_q_idx(atomic_read_u32(&evtq->h))].seq;
		rmb();
		seq_v = atomic_read_u32(seq);
		if (atomic_read_u32(&evtq->h) >= evtq->t || evtq_h_seq_v > seq_v)
			break;
		cnt++;
		if (!cnt){
			volatile u32 *q_h = &pkt_q.h, *q_t = &pkt_q.t;
			deter_log("long wait (pkt): pkt_q: %u %u; irqs_disabled:%u in_interrupt:%x local_softirq_pending:%x\n", *q_h, *q_t, irqs_disabled(), in_interrupt(), local_softirq_pending());
		}
	}
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
#if ADVANCED_EVENT_ENABLE
#define str_write(s, fmt, ...) ({ int ret; ret = sprintf((s), fmt, ##__VA_ARGS__); s+=ret; ret;})
static inline void replay_advanced_event(const struct sock *sk, u8 type, u8 loc, u8 fmt, int n, ...){
	struct DeterReplayer *r = sk->replayer;
	va_list vl;
	char name0[32];
	char buf[256], *s = buf;
	u32 h = r->aeq.h, *v = r->aeq.v;
	if (h < r->aeq.t){
		struct AdvancedEvent *ae = (struct AdvancedEvent*)(v+get_aeq_idx(h));
		int mismatch = 0, data_mismatch = 0;
		u32 i, j;

		// test if mismatch
		if (ae->type != type){
			str_write(s, "Mismatch: %u-th ae type=%s[%hhu] data:", r->aeq.i, get_ae_name(type,name0), loc);
			mismatch = 1;
		}else if (ae->loc != loc){
			str_write(s, "Mismatch: %u-th ae loc=%hhu data:", r->aeq.i, loc);
			mismatch = 1;
		}else {
			va_start(vl, n);
			for (i = 1, j = 1 << (n - 1); j; j >>= 1){
				if (fmt & j){
					u64 a = va_arg(vl, u64), b = v[get_aeq_idx(h+i)] | ((u64)v[get_aeq_idx(h+i+1)] << 32);
					data_mismatch |= (a != b);
					i+=2;
				}else {
					u32 a = va_arg(vl, u32), b = v[get_aeq_idx(h+i)];
					data_mismatch |= (a != b);
					i+=1;
				}
			}
			if (data_mismatch){
				str_write(s, "Mismatch: %u-th ae data:", r->aeq.i);
			}
			va_end(vl);
			// update h
			r->aeq.h += i;
		}

		// print data if mismatch
		if (mismatch || data_mismatch){
			va_start(vl, n);
			for (j = 1 << (n - 1); j; j >>= 1){
				if (fmt & j){
					long a = va_arg(vl, long);
					str_write(s, " %ld", a);
				}else {
					u32 a = va_arg(vl, u32);
					str_write(s, " %u", a);
				}
			}
			va_end(vl);
			deter_log("%s\n", buf);

		}

		// mismatch means we did not go through this ae in the previous test, so we did not update h there. so we update here
		if (mismatch){
			int add = 1 + ae->n;
			for (j = 1 << (ae->n - 1); j; j >>= 1)
				add += !!(ae->fmt & j);
			r->aeq.h += add;
		}

		// update aeq->i
		r->aeq.i++;
	}else
		deter_log("Mismatch: more ae than recorded. %s %hhu\n", get_ae_name(type, name0), loc);
}
#endif
static inline void new_event(struct sock *sk, u32 type){
	struct DeterReplayer *r = (struct DeterReplayer*)sk->replayer;
	if (!r)
		return;

	if (type >= DETER_SOCK_ID_BASE){
		u32 sc_id = (type - DETER_SOCK_ID_BASE) & 0x0fffffff;
		u32 loc = (type - DETER_SOCK_ID_BASE) >> 28;
		deter_log("After lock: sockcall %u (%u %u)\n", sc_id, loc >> 1, loc & 1);
	}else 
		deter_log("After lock: event type %u\n", type);

	#if DETER_DEBUG
	// check dbg_data
	if (type >= DETER_SOCK_ID_BASE){
		if (atomic_read_u32(&r->evtq.h) < r->evtq.t){
			u32 ori = r->evtq.v[get_event_q_idx(atomic_read_u32(&r->evtq.h))].dbg_data, rep = tcp_sk(sk)->write_seq;
			if (ori != rep)
				deter_log("Mismatch: write_seq %u != %u\n", ori, rep);
		}
	}
	#endif

	// NOTE: seq++ has to be before h++
	// because if h++ first, then for a very short time period (after h++ and before seq++),
	// seq < evtq.v[evtq.h].seq. This would allow a packet MISTAKENLY entering lock.
	// On the other hand, seq++ first would not have such problem, because the short time perior is seq > evtq.v[evtq.h].seq, which does not allow any thing to happen
	atomic_inc_u32(&r->seq);
	wmb();
	atomic_inc_u32(&r->evtq.h);
}

static void sockcall_lock(struct sock *sk, u32 sc_id){
	new_event(sk, sc_id + DETER_SOCK_ID_BASE);
}

static void incoming_pkt(struct sock *sk){
	struct DeterReplayer *r = (struct DeterReplayer*)sk->replayer;
	if (!r)
		return;
	deter_log("After lock: incoming pkt\n");
	atomic_inc_u32(&r->seq);
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

static void record_fin_seq(struct sock *sk){
	struct DeterReplayer *r = (struct DeterReplayer*)sk->replayer;
	if (!r)
		return;
	r->pkt_idx.fin_v64 = tcp_sk(sk)->write_seq | (0x100000000);
}

/************************************
 * read values
 ***********************************/
static unsigned long _replay_jiffies(const struct sock *sk, int id){
	struct jiffies_q *jfq = &((struct DeterReplayer*)sk->replayer)->jfq;
	#if ADVANCED_EVENT_ENABLE
	replay_advanced_event(sk, -1, id, 0b0, 1, jfq->h);
	#endif
	if (jfq->h >= jfq->t){
		deter_log("Warning: more jiffies reads then recorded %d\n", id);
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
static unsigned long replay_jiffies(const struct sock *sk, int id){
	return _replay_jiffies(sk, id + 100);
}
static unsigned long replay_tcp_time_stamp(const struct sock *sk, int id){
	return _replay_jiffies(sk, id);
}

static bool replay_tcp_under_memory_pressure(const struct sock *sk){
	struct memory_pressure_q *mpq = &((struct DeterReplayer*)sk->replayer)->mpq;
	bool ret;
	if ((ret = (mpq->h < mpq->t && mpq->v[get_mp_q_idx(mpq->h)] == mpq->seq)))
		mpq->h++;
	mpq->seq++;
	#if ADVANCED_EVENT_ENABLE
	replay_advanced_event(sk, -2, 0, 0b0, 1, (int)ret);
	#endif
	return ret;
}

static long replay_sk_memory_allocated(const struct sock *sk){
	struct memory_allocated_q *maq = &((struct DeterReplayer*)sk->replayer)->maq;
	if (maq->h >= maq->t){
		deter_log("Warning: more memory_allocated reads than recorded\n");
		return maq->last_v;
	}
	#if ADVANCED_EVENT_ENABLE
	replay_advanced_event(sk, -3, 0, 0b0, 1, maq->h);
	#endif
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

static int replay_sk_socket_allocated_read_positive(struct sock *sk){
	deter_log("Warning: replay_sk_socket_allocated_read_positive() not implemented\n");
	#if ADVANCED_EVENT_ENABLE
	replay_advanced_event(sk, -4, 0, 0b0, 1, 0);
	#endif
	return 1;
}

static void replay_skb_mstamp_get(struct sock *sk, struct skb_mstamp *cl, int loc){
	struct mstamp_q *msq = &((struct DeterReplayer*)sk->replayer)->msq;
	if (msq->h >= msq->t){
		deter_log("Warning: more skb_mstamp_get than recorded %d\n", loc);
		*cl = msq->v[get_mstamp_q_idx(msq->t - 1)];
		return;
	}
	#if ADVANCED_EVENT_ENABLE
	replay_advanced_event(sk, -5, 0, 0b0, 1, msq->h);
	#endif
	*cl = msq->v[get_mstamp_q_idx(msq->h)];
	msq->h++;
}

static bool replay_effect_bool(const struct sock *sk, int loc){
	struct effect_bool_q *ebq = &((struct DeterReplayer*)sk->replayer)->ebq[loc];
	u32 idx, bit_idx, arr_idx;
	#if ADVANCED_EVENT_ENABLE
	if (loc != 0) // loc 0 is not serializable among all events, but just within incoming packets
		replay_advanced_event(sk, -6, loc, 0b0, 1, ebq->h);
	#endif
	if (ebq->h >= ebq->t){
		deter_log("Warning: more effect_bool %d than recorded\n", loc);
		idx = get_eb_q_idx(ebq->t - 1);
	}else{
		idx = get_eb_q_idx(ebq->h);
		ebq->h++;
	}
	bit_idx = idx & 31;
	arr_idx = idx / 32;
	return (ebq->v[arr_idx] >> bit_idx) & 1;
}

static bool replay_skb_still_in_host_queue(const struct sock *sk, const struct sk_buff *skb){
	struct SkbInQueueQ *siqq = &((struct DeterReplayer*)sk->replayer)->siqq;
	bool ret;
	#if ADVANCED_EVENT_ENABLE
	replay_advanced_event(sk, -7, 0, 0b0, 1, siqq->h);
	#endif
	if (siqq->h >= siqq->t){
		deter_log("Warning: more skb_still_in_host_queue than recorded\n");
		ret = false; // usually this is false
	}else {
		u32 idx = get_siq_q_idx(siqq->h);
		siqq->h++;
		ret = (siqq->v[idx / 32] >> (idx & 31)) & 1;
	}

	// if true, the kernel code will skip the retransmission, so just return true
	if (ret)
		return ret;
	// if false, the skb should not be in the queue. So we should wait until the skb is REALLY not in the queue
	while (skb_fclone_busy(sk, skb)){
		deter_log("skb_still_in_host_queue %u: false => true; ipid:%hu\n", siqq->h - 1, ntohs(ip_hdr(skb)->id));
		cond_resched_softirq(); // we need to allow bh, because freeing skb is in softirq
	}
	return ret;
}

static int kernel_log(const struct sock *sk, const char *fmt, ...){
	int ret = 0;
	va_list args;
	if (sk->replayer == NULL)
		return 0;
	va_start(args, fmt);
	ret = deter_log_va(fmt, args);
	va_end(args);
	return ret;
}

/****************************************
 * Packet corrector
 ***************************************/
struct PacketReorderBuf{
	struct sk_buff *skb;
	struct nf_hook_state state;
	u32 idx;
};
struct PacketReorderBuf pkt_reorder_buf[65536]; // buffer for reordering

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
	struct DeterReplayer *rep;
	u32 idx, i;
	u16 ipid;
	bool continue_check = true;
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);

	// if this packet should not be replayed, skip
	if (!should_replay_skb(skb))
		return NF_ACCEPT;

	rep = replay_ops.replayer;
	{
		deter_log("Received pkt: ipid: %hu seq: %u ack: %u\n", ntohs(iph->id), ntohl(tcph->seq), ntohl(tcph->ack_seq));
	}

	// if this packet ackes our fin, skip. ipid from the other side may not be consecutive after acking our fin
	if (rep->pkt_idx.fin && ntohl(tcph->ack_seq) == rep->pkt_idx.fin_seq + 1)
		goto enqueue;

	ipid = ntohs(iph->id);

	if (rep->pkt_idx.first){
		// this is the first valid ipid for client side, record and return
		rep->pkt_idx.init_ipid = rep->pkt_idx.last_ipid = ipid;
		rep->pkt_idx.first = 0;
		// TODO: check CE bit
		goto enqueue;
	}

	idx = rep->pkt_idx.idx++;
	// TODO: check wrong drop

	// check current pkt and buffered pkt
	while (continue_check){
		// get expected ipid of next pkt
		u16 next_ipid;
		if (rep->ps.n_consec == 0){
			if (rep->ps.h < rep->ps.t){
				if ((rep->ps.v[rep->ps.h] >> 15)){ // non-1 gap
					rep->ps.n_consec = 1;
					if (rep->ps.v[rep->ps.h] == 0xffff){ // next u16 is gap
						rep->ps.h++;
						rep->ps.gap = rep->ps.v[rep->ps.h];
					}else if ((rep->ps.v[rep->ps.h] >> 14) & 1) // backward gap
						rep->ps.gap = - (rep->ps.v[rep->ps.h] & 0x3fff);
					else // forward gap
						rep->ps.gap = (rep->ps.v[rep->ps.h] & 0x3fff);
				}else{ // gap = 1
					rep->ps.n_consec = rep->ps.v[rep->ps.h];
					rep->ps.gap = 1;
				}
				rep->ps.h++;
			}else { // no more pkt to receive: should drop this and all buffered pkt
				// drop this pkt
				kfree_skb(skb);
				// drop all buffered pkt
				for (i = 0; i < 65536; i++){
					struct PacketReorderBuf *buf = &pkt_reorder_buf[i];
					if (buf->skb){
						kfree_skb(buf->skb);
						buf->skb = NULL;
					}
				}
				return NF_STOLEN;
			}
		}
		next_ipid = (u16)rep->pkt_idx.last_ipid + rep->ps.gap;
		deter_log("next_ipid: %hu gap: %hd n_consec: %hu\n", next_ipid, rep->ps.gap, rep->ps.n_consec);

		if (skb){ // check the incoming pkt
			if (ipid == next_ipid){
				rep->pkt_idx.last_ipid = next_ipid;
				enqueue(&pkt_q, skb, state);
				skb = NULL;
			}else { // put to reorder buf
				struct PacketReorderBuf *buf = &pkt_reorder_buf[ipid];
				// drop the old one
				if (buf->skb != NULL)
					kfree_skb(buf->skb);
				// put to buf
				buf->skb = skb;
				buf->state = *state;
				buf->idx = idx;
				// stop check
				continue_check = false;
			}
		}else{ // check reorder buf
			// test if next pkt is in reorder buffer
			struct PacketReorderBuf *buf = &pkt_reorder_buf[next_ipid];
			if (buf->skb != NULL){
				if (buf->idx + 32768 < idx){ // too old, regard as drop
					kfree_skb(buf->skb);
					continue_check = false;
				}else{ // next pkt is in buffer, enqueue it
					rep->pkt_idx.last_ipid = next_ipid;
					enqueue(&pkt_q, buf->skb, &buf->state);
				}
				// clear this buf
				buf->skb = NULL;
				memset(&buf->state, 0, sizeof(struct nf_hook_state));
				buf->idx = 0;
			}else
				continue_check = false;
		}

		// update state
		if (continue_check){ // if pass the check, n_consec--
			rep->ps.n_consec--;
		}
	}
	return NF_STOLEN;

enqueue:
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
	memset(pkt_reorder_buf, 0, sizeof(pkt_reorder_buf));
	if (nf_register_hook(&pc_nf_hook_ops)){
		deter_log("Cannot register packet corrector's netfilter hook\n");
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

	deter_record_ops.recorder_destruct = recorder_destruct;
	deter_record_ops.new_sendmsg = new_sendmsg;
	//deter_record_ops.new_sendpage = new_sendpage;
	deter_record_ops.new_recvmsg = new_recvmsg;
	deter_record_ops.new_close = new_close;
	deter_record_ops.new_setsockopt = new_setsockopt;
	deter_record_ops.sockcall_lock = sockcall_lock;
	deter_record_ops.incoming_pkt = incoming_pkt;
	deter_record_ops.write_timer = write_timer;
	deter_record_ops.delack_timer = delack_timer;
	deter_record_ops.keepalive_timer = keepalive_timer;
	deter_record_ops.tasklet = tasklet;
	deter_record_ops.record_fin_seq = record_fin_seq;

	deter_record_ops.sockcall_before_lock = sockcall_before_lock;
	deter_record_ops.incoming_pkt_before_lock = incoming_pkt_before_lock;
	deter_record_ops.write_timer_before_lock = write_timer_before_lock;
	deter_record_ops.delack_timer_before_lock = delack_timer_before_lock;
	deter_record_ops.keepalive_timer_before_lock = keepalive_timer_before_lock;
	deter_record_ops.tasklet_before_lock = tasklet_before_lock;
	deter_record_ops.to_replay_server = to_replay_server;

	deter_record_ops.replay_jiffies = replay_jiffies;
	deter_record_ops.replay_tcp_time_stamp = replay_tcp_time_stamp;
	deter_record_ops.replay_tcp_under_memory_pressure = replay_tcp_under_memory_pressure;
	deter_record_ops.replay_sk_under_memory_pressure = replay_tcp_under_memory_pressure;
	deter_record_ops.replay_sk_memory_allocated = replay_sk_memory_allocated;
	deter_record_ops.replay_sk_socket_allocated_read_positive = replay_sk_socket_allocated_read_positive;
	deter_record_ops.skb_mstamp_get = replay_skb_mstamp_get;
	deter_record_ops.replay_skb_still_in_host_queue = replay_skb_still_in_host_queue;
	#if ADVANCED_EVENT_ENABLE
	advanced_event = replay_advanced_event;
	#endif
	deter_record_ops.log = kernel_log;
	deter_replay_effect_bool = replay_effect_bool;

	/* The recorder_create functions must be bind last, because they are the enabler of record */
	deter_record_ops.server_recorder_create = server_recorder_create;
	deter_record_ops.client_recorder_create = client_recorder_create;
	return 0;
}

void unbind_replay_ops(void){
	stop_packet_corrector();
	deter_replay_effect_bool = NULL;
	deter_record_ops = deter_record_ops_default;
}

static void initialize_replay(struct DeterReplayer *r){
	int i;

	// init sockcall_id
	r->seq = 0;
	atomic_set(&r->sockcall_id, 0);

	// initialize evtq
	r->evtq.h = 0;

	// initialize ps
	r->ps.h = 0;
	r->ps.n_consec = r->ps.gap = 0;

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
	for (i = 0; i < DETER_EFFECT_BOOL_N_LOC; i++)
		r->ebq[i].h = 0;
}

static void finish_sock(struct sock *sk){
	int i;
	struct DeterReplayer *r = sk->replayer;
	if (!r)
		return;
	for (i = 0; i < 65536; i++){
		struct PacketReorderBuf *buf = &pkt_reorder_buf[i];
		if (buf->skb){
			kfree_skb(buf->skb);
			buf->skb = NULL;
		}
	}
}

int replay_ops_start(struct DeterReplayer *addr){
	replay_ops.replayer = addr;

	// initialize data
	initialize_replay(replay_ops.replayer);

	// bind ops
	if (bind_replay_ops())
		return -1;

	deter_log("create kthread\n");
	// start the kthread
	replay_task = kthread_run(replay_kthread, NULL, "replay_kthread");

	return 0;
}

void replay_ops_stop(void){
	if (replay_ops.state <= STARTED)
		replay_ops.state = SHOULD_STOP;

	deter_log("unbind_replay_ops\n");
	unbind_replay_ops();

	// wait for stop kthread to stop
	deter_log("wait for kthread to stop\n");
	while (replay_ops.state != STOPPED);

	// print some stats
	logging_stats(replay_ops.replayer);

	// clean remaining packets in the queue
	while (!dequeue(&pkt_q))
		deter_log("sent a leftover pkt\n");
}
