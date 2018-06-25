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
	u64 thread_id;
};
static inline const char* get_sockcall_str(struct derand_rec_sockcall *sc, char* buf){
	switch (sc->type){
		case DERAND_SOCKCALL_TYPE_SENDMSG:
			sprintf(buf, "sendmsg");
			break;
		case DERAND_SOCKCALL_TYPE_RECVMSG:
			sprintf(buf, "recvmsg");
			break;
	}
	return buf;
}
#define get_sockcall_idx(type) (((type) - DERAND_SOCK_ID_BASE) & 0x0fffffff)

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
static inline char* get_event_name(u32 type, char* buf){
	uint32_t idx, loc;
	switch (type){
		case EVENT_TYPE_PACKET:
			sprintf(buf, "pkt");
			break;
		case EVENT_TYPE_TASKLET:
			sprintf(buf, "tasklet");
			break;
		case EVENT_TYPE_WRITE_TIMEOUT:
			sprintf(buf, "write_timeout");
			break;
		case EVENT_TYPE_DELACK_TIMEOUT:
			sprintf(buf, "delack_timeout");
			break;
		case EVENT_TYPE_KEEPALIVE_TIMEOUT:
			sprintf(buf, "keepalive_timeout");
			break;
		case EVENT_TYPE_FINISH:
			sprintf(buf, "finish");
			break;
		default:
			idx = (type - DERAND_SOCK_ID_BASE) & 0x0fffffff;
			loc = (type - DERAND_SOCK_ID_BASE) >> 28;
			sprintf(buf, "sockcall %u (%u %u)", idx, loc>>1, loc&1);
	}
	return buf;
}

/* struct for maintaining pkt idx */
struct PktIdx{
	u16 init_ipid, last_ipid; // the ipid of the first packet, the ipid of the last packet
	u32 idx; // the idx of the last packet
	u8 fin; // if fin has appeared
};
static inline u16 get_pkt_idx_gap(struct PktIdx *pkt_idx, u16 ipid){
	return (u16)(ipid - pkt_idx->last_ipid);
}
static inline u32 update_pkt_idx(struct PktIdx *pkt_idx, u16 ipid){
	pkt_idx->idx += get_pkt_idx_gap(pkt_idx, ipid);
	pkt_idx->last_ipid = ipid;
	return pkt_idx->idx;
}

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
	u32 type; // 0: evtq; 1: jfq; 2: mpq; 3: maq; 4: saq; 5: msq; 6 ~ 6+DERAND_EFFECT_BOOL_N_LOC-1: ebq
	u32 data[2];
};
static inline const char* get_ge_name(u32 type, char* buf){
	switch (type){
		case 0: 
			sprintf(buf, "evtq");
			break;
		case 1:
			sprintf(buf, "jfq");
			break;
		case 2:
			sprintf(buf, "mpq");
			break;
		case 3:
			sprintf(buf, "maq");
			break;
		case 4:
			sprintf(buf, "saq");
			break;
		case 5:
			sprintf(buf, "msq");
			break;
		default:
			if (type < 1000)
				sprintf(buf, "ebq %u", type - 6);
			else 
				sprintf(buf, "loc %u", type - 1000);
			break;
	}
	return buf;
}

#endif /* _SHARED_DATA_STRUCT__BASE_STRUCT_H */
