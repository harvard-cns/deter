#ifndef _DERAND_RECORDER_H
#define _DERAND_RECORDER_H

#include <linux/types.h>
#include <linux/slab.h>

/* Different types of socket calls' ID starts with different highest 4 bits */
#define DERAND_SOCK_ID_BASE 100

#define DERAND_SOCKCALL_TYPE_SENDMSG 0
struct derand_rec_sockcall_tcp_sendmsg{
	u8 type;
	int flags; // msg.msg_flags
	size_t size;
};
#define DERAND_SOCKCALL_TYPE_RECVMSG 1
struct derand_rec_sockcall_tcp_recvmsg{
	u8 type;
	int flags; // nonblock & flags
	size_t size;
};

struct derand_rec_sockcall{
	union{
		u8 type;
		struct derand_rec_sockcall_tcp_sendmsg sendmsg;
		struct derand_rec_sockcall_tcp_recvmsg recvmsg;
	};
};

/* struct for each lock acquiring event */
struct derand_event{
	u32 seq;
	u32 type; // 0: pkt; 1: tsq; 2~99: timeout types; 100 ~ inf: socket call IDs
};

#define DERAND_EVENT_PER_SOCK 1024
#define DERAND_SOCKCALL_PER_SOCK 1024

struct derand_recorder{
	u32 seq; // current seq #
	atomic_t sockcall_id; // current socket call ID
	u32 evt_h, evt_t;
	struct derand_event evts[DERAND_EVENT_PER_SOCK]; // sequence of derand_event
	u32 sc_h, sc_t;
	struct derand_rec_sockcall sockcalls[DERAND_SOCKCALL_PER_SOCK]; // sockcall
};

#endif /* _DERAND_RECORDER_H */
