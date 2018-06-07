#ifndef _DERAND_H
#define _DERAND_H

#include <stdint.h>

/* Different types of socket calls' ID starts with different highest 4 bits */
#define DERAND_SOCK_ID_BASE 100

#define DERAND_SOCKCALL_TYPE_SENDMSG 0
struct derand_rec_sockcall_tcp_sendmsg{
	uint8_t type;
	int flags; // msg.msg_flags
	size_t size;
};
#define DERAND_SOCKCALL_TYPE_RECVMSG 1
struct derand_rec_sockcall_tcp_recvmsg{
	uint8_t type;
	int flags; // nonblock & flags
	size_t size;
};

struct derand_rec_sockcall{
	union{
		uint8_t type;
		struct derand_rec_sockcall_tcp_sendmsg sendmsg;
		struct derand_rec_sockcall_tcp_recvmsg recvmsg;
	};
};

#define EVENT_TYPE_TASKLET 1
#define EVENT_TYPE_WRITE_TIMEOUT 2
#define EVENT_TYPE_DELACK_TIMEOUT 3
#define EVENT_TYPE_KEEPALIVE_TIMEOUT 4
/* struct for each lock acquiring event */
struct derand_event{
	uint32_t seq;
	uint32_t type; // 0: pkt; 1: tsq; 2~99: timeout types; 100 ~ inf: socket call IDs
};

#define DERAND_EVENT_PER_SOCK 65536
#define DERAND_SOCKCALL_PER_SOCK 65536

struct atomic_t {
	int counter;
};
struct derand_recorder{
	volatile uint32_t sip, dip;
	volatile uint16_t sport, dport;
	volatile uint32_t recorder_id; // indicating how many sockets have used this recorder
	volatile uint32_t seq; // current seq #
	volatile atomic_t sockcall_id; // current socket call ID
	volatile uint32_t evt_h, evt_t;
	struct derand_event evts[DERAND_EVENT_PER_SOCK]; // sequence of derand_event
	volatile uint32_t sc_h, sc_t;
	struct derand_rec_sockcall sockcalls[DERAND_SOCKCALL_PER_SOCK]; // sockcall
};

#define get_sc_q_idx(i) ((i) & (DERAND_SOCKCALL_PER_SOCK - 1))
#define get_evt_q_idx(i) ((i) & (DERAND_EVENT_PER_SOCK - 1))

#endif /* _NET_DERAND_H */
