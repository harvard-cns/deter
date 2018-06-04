#include <net/tcp.h>
#include <net/derand.h>
#include <net/derand_ops.h>
#include "derand_ctrl.h"

// allocate memory for a socket
void* derand_alloc_mem(void){
	void* ret = NULL;
	int n;

	spin_lock_bh(&derand_ctrl.lock);
	if (derand_ctrl.top == 0)
		goto out;

	n = derand_ctrl.stack[--derand_ctrl.top];
	ret = derand_ctrl.addr + n * sizeof(struct derand_recorder);
out:
	spin_unlock_bh(&derand_ctrl.lock);
	return ret;
}

/* create a derand_recorder.
 * Called when a connection is successfully built
 * i.e., after receiving SYN-ACK at client and after receiving ACK at server.
 * So this function is called in bottom-half, so we must not block*/
void recorder_create(struct sock *sk){
	// create derand_recorder
	struct derand_recorder *rec = derand_alloc_mem();
	if (!rec){
		printk("[recorder_create] sport = %hu, dport = %hu, fail to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, derand_ctrl.top);
		goto out;
	}
	printk("[recorder_create] sport = %hu, dport = %hu, succeed to create recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, derand_ctrl.top);
	sk->recorder = rec;

	// init variables
	rec->seq = 0;
	atomic_set(&rec->sockcall_id, 0);
	rec->evt_h = rec->evt_t = 0;
	rec->sc_h = rec->sc_t = 0;
out:
	return;
}

void server_recorder_create(struct sock *sk){
	if (inet_sk(sk)->inet_sport == 0x8913){
		printk("server sport = %hu, dport = %hu, creating recorder\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		recorder_create(sk);
	}
}
void client_recorder_create(struct sock *sk){
	printk("client sport = %hu, dport = %hu\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
}

// recycle memory for a socket
void derand_recycle_mem(void* addr){
	spin_lock_bh(&derand_ctrl.lock);
	derand_ctrl.stack[derand_ctrl.top++] = (addr - derand_ctrl.addr) / sizeof(struct derand_recorder);
	spin_unlock_bh(&derand_ctrl.lock);
}
/* destruct a derand_recorder.
 */
void recorder_destruct(struct sock *sk){
	if (!sk->recorder){
		printk("[recorder_destruct] sport = %hu, dport = %hu, recorder is NULL.\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport);
		return;
	}
	derand_recycle_mem(sk->recorder);
	sk->recorder = NULL;
	printk("[recorder_destruct] sport = %hu, dport = %hu, succeed to destruct a recorder. top=%d\n", inet_sk(sk)->inet_sport, inet_sk(sk)->inet_dport, derand_ctrl.top);
}

#define get_sc_q_idx(i) ((i) & (DERAND_SOCKCALL_PER_SOCK - 1))

u32 new_sendmsg(struct sock *sk, struct msghdr *msg, size_t size){
	struct derand_recorder* rec = sk->recorder;
	struct derand_rec_sockcall *rec_sc;
	int sc_id;

	if (!rec)
		return 0;

	// get sockcall ID
	sc_id = atomic_add_return(1, &rec->sockcall_id) - 1;
	// get record for storing this sockcall
	rec_sc = &rec->sockcalls[get_sc_q_idx(rec->sc_t)];
	rec->sc_t++;
	// store data for this sockcall
	rec_sc->type = DERAND_SOCKCALL_TYPE_SENDMSG;
	rec_sc->sendmsg.flags = msg->msg_flags;
	rec_sc->sendmsg.size = size;
	// return sockcall ID 
	return sc_id;
}

int bind_derand_ops(void){
	derand_record_ops.recorder_destruct = recorder_destruct;
	/* The recorder_create functions must be bind last, because they are the enabler of record */
	derand_record_ops.server_recorder_create = server_recorder_create;
	derand_record_ops.client_recorder_create = client_recorder_create;
	return 0;
}
void unbind_derand_ops(void){
	derand_record_ops = derand_record_ops_default;
}
