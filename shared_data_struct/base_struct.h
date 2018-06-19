#ifndef _SHARED_DATA_STRUCT__BASE_STRUCT_H
#define _SHARED_DATA_STRUCT__BASE_STRUCT_H

#include "tcp_sock_init_data.h"

#define DERAND_DEBUG 1

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

#define EVENT_TYPE_PACKET 0
#define EVENT_TYPE_TASKLET 1
#define EVENT_TYPE_WRITE_TIMEOUT 2
#define EVENT_TYPE_DELACK_TIMEOUT 3
#define EVENT_TYPE_KEEPALIVE_TIMEOUT 4
#define EVENT_TYPE_FINISH 99
/* struct for each lock acquiring event */
struct derand_event{
	u32 seq;
	u32 type; // 0: pkt; 1: tsq; 2~98: timeout types; 99: finish; 100 ~ inf: socket call IDs + DERAND_SOCK_ID_BASE
	#if DERAND_DEBUG
	u32 dbg_data;
	#endif
};

/* struct for a jiffies read with new value */
union jiffies_rec{
	unsigned long init_jiffies; // if this is the first jiffies read
	struct {
		u32 jiffies_delta;
		u32 idx_delta;
	};
};

/* struct for memory_allocated read with new value */
union memory_allocated_rec{
	long init_v;
	struct {
		s32 v_delta;
		u32 idx_delta;
	};
};

#define DERAND_EFFECT_BOOL_N_LOC 16

/* struct general event (for debug): including locking and reading */
struct GeneralEvent{
	u8 type; // 0: evtq; 1: jfq; 2: mpq; 3: maq; 4: saq; 5: msq; 6 ~ 6+DERAND_EFFECT_BOOL_N_LOC-1: ebq
};

#endif /* _SHARED_DATA_STRUCT__BASE_STRUCT_H */
