#ifndef _SHARED_DATA_STRUCT__DERAND_REPLAYER_H
#define _SHARED_DATA_STRUCT__DERAND_REPLAYER_H

#include "base_struct.h"
#include "tcp_sock_init_data.h"

#define EVENT_Q_LEN 16384
struct event_q{
	u32 h, t;
	struct derand_event v[EVENT_Q_LEN];
};
#define get_event_q_idx(i) ((i) & (EVENT_Q_LEN - 1))

#define JIFFIES_Q_LEN 8192
struct jiffies_q{
	unsigned long last_jiffies; // the last jiffies value
	u32 h, t;
	u32 idx_delta;
	union jiffies_rec v[JIFFIES_Q_LEN];
};
#define get_jiffies_q_idx(i) ((i) & (JIFFIES_Q_LEN - 1))

#define MEMORY_PRESSURE_Q_LEN 256
struct memory_pressure_q{
	u32 h, t;
	u32 seq; // seq # for memory pressure read
	u32 v[MEMORY_PRESSURE_Q_LEN]; // indexes of seq# which returns 1
};
#define get_mp_q_idx(i) ((i) & (MEMORY_PRESSURE_Q_LEN - 1))

#define MEMORY_ALLOCATED_Q_LEN 1024
struct memory_allocated_q{
	long last_v;
	u32 h, t;
	u32 idx_delta;
	union memory_allocated_rec v[MEMORY_ALLOCATED_Q_LEN];
};
#define get_ma_q_idx(i) ((i) & (MEMORY_ALLOCATED_Q_LEN - 1))

#define MSTAMP_Q_LEN 65536
struct mstamp_q{
	u32 h, t;
	struct skb_mstamp v[MSTAMP_Q_LEN];
};
#define get_mstamp_q_idx(i) ((i) & (MSTAMP_Q_LEN - 1))

#define EFFECT_BOOL_Q_LEN 8192
struct effect_bool_q{
	u32 h, t;
	u32 v[EFFECT_BOOL_Q_LEN / 32];
};
#define get_eb_q_idx(i) ((i) & (EFFECT_BOOL_Q_LEN - 1))

#if DERAND_DEBUG
#define GENERAL_EVENT_Q_LEN 65536
struct GeneralEventQ{
	u32 h, t;
	struct GeneralEvent v[GENERAL_EVENT_Q_LEN];
};
#define get_geq_idx(i) ((i) & (GENERAL_EVENT_Q_LEN - 1))
#endif /* DERAND_DEBUG */

struct derand_replayer{
	struct tcp_sock_init_data init_data; // initial values for tcp_sock
	u32 seq;
	atomic_t sockcall_id; // current socket call ID
	struct event_q evtq;
	struct jiffies_q jfq;
	struct memory_pressure_q mpq;
	struct memory_allocated_q maq;
	struct mstamp_q msq;
	struct effect_bool_q ebq[DERAND_EFFECT_BOOL_N_LOC]; // effect_bool
	#if DERAND_DEBUG
	struct GeneralEventQ geq;
	#endif
};

#endif /* _SHARED_DATA_STRUCT__DERAND_REPLAYER_H */
