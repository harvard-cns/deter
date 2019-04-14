#ifndef _SHARED_DATA_STRUCT__DETER_RECORDER_H
#define _SHARED_DATA_STRUCT__DETER_RECORDER_H

#include "base_struct.h"
#include "tcp_sock_init_data.h"
#include "mem_block.h"

#define N_MEM_BLOCK 1024
/* Note: N_RECORDER must be smaller than N_MEM_BLOCK / #MemBlock_per_recorder */
#define N_RECORDER 16

#if USE_DEPRECATED
/***********************************
 * drop
 **********************************/
#define DERAND_DROP_PER_SOCK 256
struct DropQ{
	u32 h, t;
	u32 v[DERAND_DROP_PER_SOCK];
};
#define get_drop_q_idx(i) ((i) & (DERAND_DROP_PER_SOCK - 1))

#if GET_RX_PKT_IDX
#define DERAND_RX_PKT_PER_SOCK 512
struct RxPktQ{
	u32 h, t;
	u32 v[DERAND_RX_PKT_PER_SOCK];
};
static inline u32 get_rx_pkt_q_idx(u32 i){
	return i & (DERAND_RX_PKT_PER_SOCK - 1);
}
#endif

#if GET_REORDER
/************************************
 * reorder
 ***********************************/
#define DERAND_MAX_REORDER 1024 
#define DERAND_REORDER_BUF_SIZE (DERAND_MAX_REORDER * sizeof(u16) * 2)
struct Reorder{
	u32 left, max_id, min_id;
	u8 map[DERAND_MAX_REORDER];
	u8 ascending;
	u32 buf_idx;
	struct ReorderPeriod *period;
	u32 h, t;
	u8 buf[DERAND_REORDER_BUF_SIZE + DERAND_MAX_REORDER * sizeof(u16) * 2]; // base buffer, plus headroom
};
static inline u32 get_reorder_buf_idx(u32 i){
	return i & (DERAND_REORDER_BUF_SIZE - 1);
}
#endif

/************************************
 * jiffies
 ***********************************/
#define DERAND_JIFFIES_PER_SOCK 512
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
#define DERAND_MEMORY_PRESSURE_PER_SOCK 256
/* struct for memory pressure */
struct memory_pressure_q{
	u32 h, t;
	u32 v[DERAND_MEMORY_PRESSURE_PER_SOCK / 32];
};
#define get_memory_pressure_q_idx(i) ((i) & (DERAND_MEMORY_PRESSURE_PER_SOCK - 1))

/*************************************
 * memory_allocated
 ************************************/
#define DERAND_MEMORY_ALLOCATED_PER_SOCK 256
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

/****************************************
 * skb_still_in_host_queue
 ****************************************/
#define DERAND_SKB_IN_QUEUE_PER_SOCK 64
struct SkbInQueueQ{
	u32 h, t;
	bool v[DERAND_SKB_IN_QUEUE_PER_SOCK];
};
#define get_siq_q_idx(i) ((i) & (DERAND_SKB_IN_QUEUE_PER_SOCK - 1))

/****************************************
 * AdvancedEvent
 ***************************************/
#if ADVANCED_EVENT_ENABLE
#define DERAND_ADVANCED_EVENT_PER_SOCK 16384
struct AdvancedEventQ{
	u32 h, t;
	u32 v[DERAND_ADVANCED_EVENT_PER_SOCK]; // this is diff from other queues. Each u32 is either a AdvancedEvent, or a data
};
static inline u32 get_aeq_idx(u32 i){
	return i & (DERAND_ADVANCED_EVENT_PER_SOCK - 1);
}
#endif /* ADVANCED_EVENT_ENABLE */

#if COLLECT_TX_STAMP
#define DERAND_TX_STAMP_PER_SOCK 1024
struct TxStampQ{
	u32 h, t;
	u32 v[DERAND_TX_STAMP_PER_SOCK];
};
static inline u32 get_tsq_idx(u32 i){
	return i & (DERAND_TX_STAMP_PER_SOCK - 1);
}
#endif

#if COLLECT_RX_STAMP
#define DERAND_RX_STAMP_PER_SOCK 512
struct RxStampQ{
	u32 h, t;
	u32 v[DERAND_RX_STAMP_PER_SOCK];
};
static inline u32 get_rsq_idx(u32 i){
	return i & (DERAND_RX_STAMP_PER_SOCK - 1);
}
#endif
#endif /* USE_DEPRECATED */

struct EventState{
	u32 n;
	struct MemBlock *mb;
};
struct SockcallState{
	u32 n;
	struct MemBlock *mb;
};
struct DropState{
	u32 n;
	struct MemBlock *mb;
};
struct JiffiesState{
	unsigned long last_jiffies; // the last jiffies value
	u32 idx_delta;
	u32 n;
	struct MemBlock *mb;
};
struct MemoryPressureState{
	u32 n;
	struct MemBlock *mb;
};
struct MemoryAllocatedState{
	long last_v;
	u32 idx_delta;
	u32 n;
	struct MemBlock *mb;
};
struct MstampState{
	u32 n;
	struct MemBlock *mb;
};
struct SiqState{
	u32 n;
	struct MemBlock *mb;
};
struct TxstampState{
	u32 n;
	struct MemBlock *mb;
};
struct EffectBoolState{
	u32 n;
	struct MemBlock *mb;
};
#if ADVANCED_EVENT_ENABLE
struct AdvancedEventState{
	u32 n;
	struct MemBlock *mb;
};
#endif

#if USE_DEPRECATED
#define DERAND_EVENT_PER_SOCK 2048
#define DERAND_SOCKCALL_PER_SOCK 512
#endif

struct DeterRecorder{
	u32 broken, alert;
	u32 mode;
	u32 sip, dip;
	u16 sport, dport;
	u32 used_mb, dump_mb; // number of MemBlock being used, number of MemBlock being dumped. User space should put this recorder to the free recorder pool when used_mb==dump_mb
	struct tcp_sock_init_data init_data;
	u32 seq; // current seq #
	atomic_t sockcall_id, sockcall_id_mp; // current socket call ID, and var for multi-producer
	struct PktIdx pkt_idx; // maintain pkt idx

	struct EventState evt;
	struct SockcallState sockcall;
	struct DropState dp;
	struct JiffiesState jif;
	struct MemoryPressureState mp;
	struct MemoryAllocatedState ma;
	u32 n_sockets_allocated;
	struct MstampState ms;
	struct SiqState siq;
	#if COLLECT_TX_STAMP
	struct TxstampState ts;
	#endif
	struct EffectBoolState eb[DETER_EFFECT_BOOL_N_LOC];
	#if ADVANCED_EVENT_ENABLE
	struct AdvancedEventState ae;
	#endif

	#if USE_DEPRECATED
	u32 evt_h, evt_t;
	struct derand_event evts[DERAND_EVENT_PER_SOCK]; // sequence of derand_event
	u32 sc_h, sc_t;
	struct derand_rec_sockcall sockcalls[DERAND_SOCKCALL_PER_SOCK]; // sockcall
	struct DropQ dpq; // drop
	#if GET_REORDER
	struct Reorder reorder; // reorder
	#endif
	#if GET_RX_PKT_IDX
	struct RxPktQ rpq; // rx packet
	#endif
	struct jiffies_q jf; // jiffies
	struct memory_pressure_q mpq; // memory pressure
	struct memory_allocated_q maq; // memory_allocated
	struct mstamp_q msq; // skb_mstamp
	struct effect_bool_q ebq[DETER_EFFECT_BOOL_N_LOC]; // effect_bool
	struct SkbInQueueQ siqq; // skb_still_in_host_queue
	#if ADVANCED_EVENT_ENABLE
	struct AdvancedEventQ aeq;
	#endif
	#if COLLECT_TX_STAMP
	struct TxStampQ tsq;
	#endif
	#if COLLECT_RX_STAMP
	struct RxStampQ rsq;
	#endif
	#if GET_BOTTLENECK
	u64 net, recv, other;
	u64 app_net, app_recv, app_other;
	u64 last_bottleneck_update_time;
	#endif
	#endif /* USE_DEPRECATED */
};

// NOTE: remember to change DETER_MEM_BLOCK_TYPE_TOTAL if add other type of data

#define DETER_MEM_BLOCK_TYPE_EVT 0
#define DETER_MEM_BLOCK_TYPE_SOCKCALL 1
#define DETER_MEM_BLOCK_TYPE_DP 2
#define DETER_MEM_BLOCK_TYPE_JIF 3
#define DETER_MEM_BLOCK_TYPE_MP 4
#define DETER_MEM_BLOCK_TYPE_MA 5
#define DETER_MEM_BLOCK_TYPE_MS 6
#define DETER_MEM_BLOCK_TYPE_SIQ 7
#define DETER_MEM_BLOCK_TYPE_TS 8
#define DETER_MEM_BLOCK_TYPE_EB(i) (9 + (i))
#if ADVANCED_EVENT_ENABLE
#define DETER_MEM_BLOCK_TYPE_AE (9 + DETER_EFFECT_BOOL_N_LOC)
#define DETER_MEM_BLOCK_TYPE_TOTAL (9 + DETER_EFFECT_BOOL_N_LOC + 1)
#else
#define DETER_MEM_BLOCK_TYPE_TOTAL (9 + DETER_EFFECT_BOOL_N_LOC)
#endif

struct RecorderRing{
	u32 h, t;
	u32 v[N_RECORDER];
};
static inline u32 get_rec_ring_idx(u32 i){
	return i & (N_RECORDER - 1);
}
struct DoneMemBlockRing{
	u32 h, t;
	u32 t_mp; // tail for multi-producer, used for diff producer (kernel recorder)
	u32 v[N_MEM_BLOCK];
};
static inline u32 get_done_mb_ring_idx(u32 i){
	return i & (N_MEM_BLOCK - 1);
}
struct FreeMemBlockRing{
	u32 h, t;
	u32 v[N_MEM_BLOCK];
};
static inline u32 get_free_mb_ring_idx(u32 i){
	return i & (N_MEM_BLOCK - 1);
}

/*
 * Life time of a MemBlock:
 *   Free MemBlock ring ----need a MemBlock (Kernel)---------> recorder (pointed by recorder field)
 *            ^                                                   |
 *            |                                                   |
 *       dumped (user)                                            |
 *            |                                                   |
 *   Done MemBlock ring <----done a MemBlock (Kernel)-------------|
 *                           a full MemBlock, or the recorder is done (so put all remaining MemBlock)
 *
 * Life time of a recorder:
 *   Free recorder ring --------new sock (Kernel)------------> sock ---> referenced by each MemBlock of this recorder, ++rec.used_mb for each MemBlock (Kernel)
 *            ^                                                                                |
 *            |                                                                                |
 *            |----------------rec.used_mb==rec.dump_mb (user)---------------------------------|
 *                             user ++rec.dump_mb for each MemBlock
 *                             when used_mb==dump_mb, it means this recorder is done
 */
struct SharedMemLayout{
	// Free recorder ring. Store the index to recorder
	// Kernel get a new recorder from here upon new sock
	// User put back a finished recorder here
	struct RecorderRing free_rec_ring;

	// Free MemBlock ring. Store the index to the MemBlock
	// Kernel get a new MemBlock when need more space to store runtime data
	// User put back a dumped MemBlock here
	struct FreeMemBlockRing free_mb_ring;

	// Done MemBlock ring. Store the index to the MemBlock
	// Kernel put a done (full or sock finish) MemBlock here
	// User get a done MemBlock here, copy (dump) it, and put to free MemBlock ring
	struct DoneMemBlockRing done_mb_ring;

	// recorder buffer
	// The actual memory space for recorder
	struct DeterRecorder recorder[N_RECORDER];

	// align to MEM_BLOCK_SIZE; Or align to other size ( >= cache size and <= page size)
	uint8_t cacheline0[0] __attribute__((aligned(MEM_BLOCK_SIZE)));

	// MemBlock buffer
	// the actuall memory space for MemBlock
	struct MemBlock mem_block[N_MEM_BLOCK];
};

#if USE_DEPRECATED
#define get_sc_q_idx(i) ((i) & (DERAND_SOCKCALL_PER_SOCK - 1))
#define get_evt_q_idx(i) ((i) & (DERAND_EVENT_PER_SOCK - 1))

#define BROKEN_EVENT (1 << 0)
#define BROKEN_SOCKCALL (1 << 1)
#define BROKEN_DPQ (1 << 2)
#define BROKEN_JFQ (1 << 3)
#define BROKEN_MPQ (1 << 4)
#define BROKEN_MAQ (1 << 5)
#define BROKEN_MSQ (1 << 6)
#define BROKEN_SIQQ (1 << 7)
#define BROKEN_EBQ(i) (1 << (8 + (i)))
#define BROKEN_AEQ (1 << (8 + DERAND_EFFECT_BOOL_N_LOC))
#define BROKEN_UNSUPPORTED_SOCKOPT (1 << (10 + DERAND_EFFECT_BOOL_N_LOC))
#endif /* USE_DEPRECATED */

#endif /* _SHARED_DATA_STRUCT__DETER_RECORDER_H */
