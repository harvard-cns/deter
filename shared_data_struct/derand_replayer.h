#ifndef _SHARED_DATA_STRUCT__DERAND_REPLAYER_H
#define _SHARED_DATA_STRUCT__DERAND_REPLAYER_H

#include "base_struct.h"
#include "tcp_sock_init_data.h"

#define EVENT_Q_LEN 32768
struct event_q{
	u32 h, t;
	struct derand_event v[EVENT_Q_LEN];
};
#define get_event_q_idx(i) ((i) & (EVENT_Q_LEN - 1))

#define DROP_Q_LEN 1024
struct DropQ{
	u32 h, t;
	u32 v[DROP_Q_LEN];
};
#define get_drop_q_idx(i) ((i) & (DROP_Q_LEN - 1))

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

#define MSTAMP_Q_LEN 16384
struct mstamp_q{
	u32 h, t;
	struct skb_mstamp v[MSTAMP_Q_LEN];
};
#define get_mstamp_q_idx(i) ((i) & (MSTAMP_Q_LEN - 1))

#define EFFECT_BOOL_Q_LEN 32768
struct effect_bool_q{
	u32 h, t;
	u32 v[EFFECT_BOOL_Q_LEN / 32];
};
#define get_eb_q_idx(i) ((i) & (EFFECT_BOOL_Q_LEN - 1))

#define SKB_IN_QUEUE_Q_LEN 256
struct SkbInQueueQ{
	u32 h, t;
	bool v[SKB_IN_QUEUE_Q_LEN];
};
#define get_siq_q_idx(i) ((i) & (SKB_IN_QUEUE_Q_LEN - 1))

#if DERAND_DEBUG
#define GENERAL_EVENT_Q_LEN 65536
struct GeneralEventQ{
	u32 h, t;
	struct GeneralEvent v[GENERAL_EVENT_Q_LEN];
};
#define get_geq_idx(i) ((i) & (GENERAL_EVENT_Q_LEN - 1))
#endif /* DERAND_DEBUG */

#if ADVANCED_EVENT_ENABLE
#define ADVANCED_EVENT_Q_LEN 262144
struct AdvancedEventQ{
	u32 h, t, i;
	u32 v[ADVANCED_EVENT_Q_LEN];
};
static inline u32 get_aeq_idx(u32 i){
	return i & (ADVANCED_EVENT_Q_LEN - 1);
}
#endif /* ADVANCED_EVENT_ENABLE */

struct derand_replayer{
	u32 mode; // 0: server, 1: client
	u16 port; // the socket to replay should has this port number. Depending on mode, this is either sport (server) or dport (client)
	struct tcp_sock_init_data init_data; // initial values for tcp_sock
	u32 seq;
	atomic_t sockcall_id; // current socket call ID
	struct event_q evtq;
	struct PktIdx pkt_idx;
	struct DropQ dpq;
	struct jiffies_q jfq;
	struct memory_pressure_q mpq;
	struct memory_allocated_q maq;
	struct mstamp_q msq;
	struct effect_bool_q ebq[DERAND_EFFECT_BOOL_N_LOC]; // effect_bool
	struct SkbInQueueQ siqq; // skb_still_in_host_queue
	#if DERAND_DEBUG
	struct GeneralEventQ geq;
	#endif
	#if ADVANCED_EVENT_ENABLE
	struct AdvancedEventQ aeq;
	#endif
};

#endif /* _SHARED_DATA_STRUCT__DERAND_REPLAYER_H */
