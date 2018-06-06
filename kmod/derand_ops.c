#include <net/tcp.h>
#include <net/derand.h>
#include <net/derand_ops.h>
#include "derand_ctrl.h"
#include "derand_recorder.h"

// allocate memory for a socket
static void* derand_alloc_mem(void){
	void* ret = NULL;
	int n;

	spin_lock_bh(&derand_ctrl.lock);
	if (derand_ctrl.top == 0)
		goto out;

	n = derand_ctrl.stack[--derand_ctrl.top];
	ret = derand_ctrl.addr + n; // * sizeof(struct derand_recorder);
out:
	spin_unlock_bh(&derand_ctrl.lock);
	return ret;
}

/* create a derand_recorder.
 * Called when a connection is successfully built
 * i.e., after receiving SYN-ACK at client and after receiving ACK at server.
 * So this function is called in bottom-half, so we must not block*/
static void recorder_create(struct sock *sk){
	// create derand_recorder
	struct derand_recorder *rec = derand_alloc_mem();
	if (!rec){
		printk("[recorder_create] sport = %hu, dport = %hu, fail to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, derand_ctrl.top);
		goto out;
	}
	printk("[recorder_create] sport = %hu, dport = %hu, succeed to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, derand_ctrl.top);
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
	rec->recorder_id++;
out:
	return;
}

static void server_recorder_create(struct sock *sk){
	if (inet_sk(sk)->inet_sport == 0x8913){ // port 5001
		printk("server sport = %hu, dport = %hu, creating recorder\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		recorder_create(sk);
	}
}
static void client_recorder_create(struct sock *sk){
	if (inet_sk(sk)->inet_dport == 0x8913){ // port 5001
		printk("client sport = %hu, dport = %hu, creating recorder\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		recorder_create(sk);
	}
}

/* destruct a derand_recorder.
 */
static void recorder_destruct(struct sock *sk){
	struct derand_recorder* rec = sk->recorder;
	if (!rec){
		printk("[recorder_destruct] sport = %hu, dport = %hu, recorder is NULL.\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		return;
	}

	// update recorder_id
	rec->recorder_id++;

	// recycle this recorder
	spin_lock_bh(&derand_ctrl.lock);
	derand_ctrl.stack[derand_ctrl.top++] = (rec - derand_ctrl.addr); // / sizeof(struct derand_recorder);
	spin_unlock_bh(&derand_ctrl.lock);

	// remove the recorder from sk
	sk->recorder = NULL;
	printk("[recorder_destruct] sport = %hu, dport = %hu, succeed to destruct a recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, derand_ctrl.top);
}

static u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1 + DERAND_SOCK_ID_BASE;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)];
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_SENDMSG;
	rec_sc->sendmsg.flags = msg->msg_flags;
	rec_sc->sendmsg.size = size;
	// return sockcall ID 
	return sc_id;
}

static u32 new_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock, int flags, int *addr_len){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1 + DERAND_SOCK_ID_BASE;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(sc_id)];
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_RECVMSG;
	rec_sc->recvmsg.flags = nonblock & msg->msg_flags;
	rec_sc->recvmsg.size = len;
	// return sockcall ID 
	return sc_id;
}

static inline void new_event(struct derand_recorder *rec, u32 type){
	u32 seq;
	if (!rec)
		return;

	// increment the seq #
	seq = rec->seq++;

	// enqueue the new event
	rec->evts[get_evt_q_idx(rec->evt_t)] = (struct derand_event){.seq = seq, .type = type};
	rec->evt_t++;
}

static void sockcall_lock(struct sock *sk, u32 sc_id){
	new_event(sk->recorder, sc_id);
}

/* incoming packets do not need to record their seq # */
static void incoming_pkt(struct sock *sk){
	if (!sk->recorder)
		return;
	((struct derand_recorder*)sk->recorder)->seq++;
}

static void write_timer(struct sock *sk){
	new_event(sk->recorder, EVENT_TYPE_WRITE_TIMEOUT);
}

static void delack_timer(struct sock *sk){
	new_event(sk->recorder, EVENT_TYPE_DELACK_TIMEOUT);
}

static void keepalive_timer(struct sock *sk){
	new_event(sk->recorder, EVENT_TYPE_KEEPALIVE_TIMEOUT);
}

static void tasklet(struct sock *sk){
	new_event(sk->recorder, EVENT_TYPE_TASKLET);
}

int bind_derand_ops(void){
	derand_record_ops.recorder_destruct = recorder_destruct;
	derand_record_ops.new_sendmsg = new_sendmsg;
	derand_record_ops.new_recvmsg = new_recvmsg;
	derand_record_ops.sockcall_lock = sockcall_lock;
	derand_record_ops.incoming_pkt = incoming_pkt;
	derand_record_ops.write_timer = write_timer;
	derand_record_ops.delack_timer = delack_timer;
	derand_record_ops.keepalive_timer = keepalive_timer;
	derand_record_ops.tasklet = tasklet;

	/* The recorder_create functions must be bind last, because they are the enabler of record */
	derand_record_ops.server_recorder_create = server_recorder_create;
	derand_record_ops.client_recorder_create = client_recorder_create;
	return 0;
}
void unbind_derand_ops(void){
	derand_record_ops = derand_record_ops_default;
}
