#ifndef _SHARED_DATA_STRUCT__DETER_RECORDER_H
#define _SHARED_DATA_STRUCT__DETER_RECORDER_H

#include "base_struct.h"
#include "tcp_sock_init_data.h"
#include "mem_block.h"

#define N_MEM_BLOCK 1024
/* Note: N_RECORDER must be smaller than N_MEM_BLOCK / #MemBlock_per_recorder */
#define N_RECORDER 16

struct EventState{
	u32 n;
	struct MemBlock *mb;
};
struct SockcallState{
	u32 n;
	struct MemBlock *mb;
};
struct PktStreamState{
	u32 n;
	u16 last;
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
	struct PktStreamState ps;
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
};

// NOTE: remember to change DETER_MEM_BLOCK_TYPE_TOTAL if add other type of data

#define DETER_MEM_BLOCK_TYPE_EVT 0
#define DETER_MEM_BLOCK_TYPE_SOCKCALL 1
#define DETER_MEM_BLOCK_TYPE_PS 2
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

#endif /* _SHARED_DATA_STRUCT__DETER_RECORDER_H */
