#ifndef _SHARED_DATA_STRUCT__DERAND_RECORDER_H
#define _SHARED_DATA_STRUCT__DERAND_RECORDER_H

#include "base_struct.h"
#include "tcp_sock_init_data.h"

/***********************************
 * drop
 **********************************/
#define DERAND_DROP_PER_SOCK 1024
struct DropQ{
	u32 h, t;
	u32 v[DERAND_DROP_PER_SOCK];
};
#define get_drop_q_idx(i) ((i) & (DERAND_DROP_PER_SOCK - 1))

/************************************
 * jiffies
 ***********************************/
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

/****************************************
 * GeneralEvent
 ***************************************/
#if DERAND_DEBUG
#define DERAND_GENERAL_EVENT_PER_SOCK 8192
struct GeneralEventQ{
	u32 h, t;
	struct GeneralEvent v[DERAND_GENERAL_EVENT_PER_SOCK];
};
#define get_geq_idx(i) ((i) & (DERAND_GENERAL_EVENT_PER_SOCK - 1))
static inline void add_geq(struct GeneralEventQ *geq, u32 type, u64 data){
	struct GeneralEvent *ge = &geq->v[get_geq_idx(geq->t)];
	ge->type = type;
	*(u64*)(ge->data) = data;
	geq->t++;
}
#endif /* DERAND_DEBUG */

#define DERAND_EVENT_PER_SOCK 4096
#define DERAND_SOCKCALL_PER_SOCK 4096

struct derand_recorder{
	u32 mode;
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
	struct PktIdx pkt_idx; // maintain pkt idx
	struct DropQ dpq; // drop
	struct jiffies_q jf; // jiffies
	struct memory_pressure_q mpq; // memory pressure
	struct memory_allocated_q maq; // memory_allocated
	u32 n_sockets_allocated;
	struct mstamp_q msq; // skb_mstamp
	struct effect_bool_q ebq[DERAND_EFFECT_BOOL_N_LOC]; // effect_bool
	#if DERAND_DEBUG
	struct GeneralEventQ geq;
	#endif
};

#define get_sc_q_idx(i) ((i) & (DERAND_SOCKCALL_PER_SOCK - 1))
#define get_evt_q_idx(i) ((i) & (DERAND_EVENT_PER_SOCK - 1))

#endif /* _SHARED_DATA_STRUCT__DERAND_RECORDER_H */
