#ifndef _SHARED_DATA_STRUCT__DERAND_RECORDER_H
#define _SHARED_DATA_STRUCT__DERAND_RECORDER_H

#include "tcp_sock_init_data.h"

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

#define EVENT_TYPE_TASKLET 1
#define EVENT_TYPE_WRITE_TIMEOUT 2
#define EVENT_TYPE_DELACK_TIMEOUT 3
#define EVENT_TYPE_KEEPALIVE_TIMEOUT 4
/* struct for each lock acquiring event */
struct derand_event{
	u32 seq;
	u32 type; // 0: pkt; 1: tsq; 2~99: timeout types; 100 ~ inf: socket call IDs + 100
};

/************************************
 * jiffies
 ***********************************/
/* struct for a jiffies read with new value */
union jiffies_rec{
	unsigned long init_jiffies; // if this is the first jiffies read
	struct {
		s32 jiffies_delta;
		u32 idx_delta;
	};
};

#define DERAND_JIFFIES_PER_SOCK 1024 
struct jiffies_q{
	unsigned long last_jiffies; // the last jiffies value
	u32 h, t;
	u32 idx_delta;
	union jiffies_rec v[DERAND_JIFFIES_PER_SOCK];
};
#define get_jiffies_q_idx(i) ((i) & (DERAND_JIFFIES_PER_SOCK - 1))

/*************************************
 * memory_pressure
 ************************************/
#define DERAND_MEMORY_PRESSURE_PER_SOCK 8192
/* struct for memory pressure */
struct memory_pressure_q{
	u32 h, t;
	u32 v[DERAND_MEMORY_PRESSURE_PER_SOCK / 32];
};
#define get_memory_pressure_q_idx(i) ((i) & (DERAND_MEMORY_PRESSURE_PER_SOCK - 1))
static inline void push_memory_pressure_q(struct memory_pressure_q *q, bool v){
	u32 idx = get_memory_pressure_q_idx(q->t);
	u32 bit_idx = idx & 31, arr_idx = idx / 32;
	q->t++;
	if (bit_idx == 0)
		q->v[arr_idx] = (u32)v;
	else
		q->v[arr_idx] |= ((u32)v) << bit_idx;
}

/*************************************
 * memory_allocated
 ************************************/
#define DERAND_MEMORY_ALLOCATED_PER_SOCK 1024
union memory_allocated_rec{
	long init_v;
	struct {
		u32 v_delta;
		u32 idx_delta;
	};
};
/* struct for memory_allocated */
struct memory_allocated_q{
	long last_v;
	u32 h, t;
	u32 idx_delta;
	union memory_allocated_rec v[DERAND_MEMORY_ALLOCATED_PER_SOCK];
};
#define get_memory_allocated_q_idx(i) ((i) & (DERAND_MEMORY_ALLOCATED_PER_SOCK - 1))

/****************************************
 * skb_mstamp
 ***************************************/
#define DERAND_MSTAMP_PER_SOCK 4096
struct mstamp_q{
	u32 h, t;
	struct skb_mstamp v[DERAND_MSTAMP_PER_SOCK];
};
#define get_mstamp_q_idx(i) ((i) & (DERAND_MSTAMP_PER_SOCK - 1))

/****************************************
 * effect_bool
 ****************************************/
#define DERAND_EFFECT_BOOL_PER_SOCK 2048
struct effect_bool_q{
	u32 h, t;
	u32 v[DERAND_EFFECT_BOOL_PER_SOCK / 32];
};
#define get_effect_bool_q_idx(i) ((i) & (DERAND_EFFECT_BOOL_PER_SOCK - 1))
static inline void push_effect_bool_q(struct effect_bool_q *q, bool v){
	u32 idx = get_effect_bool_q_idx(q->t);
	u32 bit_idx = idx & 31, arr_idx = idx / 32;
	q->t++;
	if (bit_idx == 0)
		q->v[arr_idx] = (u32)v;
	else
		q->v[arr_idx] |= ((u32)v) << bit_idx;
}

#define DERAND_EVENT_PER_SOCK 4096
#define DERAND_SOCKCALL_PER_SOCK 4096
#define DERAND_EFFECT_BOOL_N_LOC 16

struct derand_recorder{
	u32 sip, dip;
	u16 sport, dport;
	u32 recorder_id; // indicating how many sockets have used this recorder
	struct tcp_sock_init_data init_data; // initial values for tcp_sock
	u32 seq; // current seq #
	atomic_t sockcall_id; // current socket call ID
	u32 evt_h, evt_t;
	struct derand_event evts[DERAND_EVENT_PER_SOCK]; // sequence of derand_event
	u32 sc_h, sc_t;
	struct derand_rec_sockcall sockcalls[DERAND_SOCKCALL_PER_SOCK]; // sockcall
	struct jiffies_q jf; // jiffies
	struct memory_pressure_q mpq; // memory pressure
	struct memory_allocated_q maq; // memory_allocated
	u32 n_sockets_allocated;
	struct mstamp_q msq; // skb_mstamp
	struct effect_bool_q ebq[DERAND_EFFECT_BOOL_N_LOC]; // effect_bool
};

#define get_sc_q_idx(i) ((i) & (DERAND_SOCKCALL_PER_SOCK - 1))
#define get_evt_q_idx(i) ((i) & (DERAND_EVENT_PER_SOCK - 1))

#endif /* _SHARED_DATA_STRUCT__DERAND_RECORDER_H */
