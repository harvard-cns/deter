#ifndef _SHARED_DATA_STRUCT__BASE_STRUCT_H
#define _SHARED_DATA_STRUCT__BASE_STRUCT_H

#include "tcp_sock_init_data.h"

#define DERAND_DEBUG 0
#define ADVANCED_EVENT_ENABLE 0
#define COLLECT_TX_STAMP 1
#define COLLECT_RX_STAMP 0
#define GET_EVENT_STAMP 0
#define GET_CWND 0
#define GET_RX_PKT_IDX 1

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
	int flags; // nonblock | flags
	size_t size;
};
#define DERAND_SOCKCALL_TYPE_CLOSE 2
struct derand_rec_sockcall_tcp_close{
	u8 type;
	long timeout;
};
#define DERAND_SOCKCALL_TYPE_SPLICE_READ 3
struct derand_rec_sockcall_tcp_splice_read{
	u8 type;
	int flags;
	size_t size;
};
#define DERAND_SOCKCALL_TYPE_SETSOCKOPT 4
struct derand_rec_sockcall_setsockopt{
	u8 type;
	u8 level;
	u8 optname;
	u8 optlen;
	u8 optval[12];
};
static inline bool valid_rec_setsockopt(struct derand_rec_sockcall_setsockopt *sc){
	return sc->level < 255 || sc->optname < 255 || sc->optlen <= 12;
}

struct derand_rec_sockcall{
	union{
		u8 type;
		struct derand_rec_sockcall_tcp_sendmsg sendmsg;
		struct derand_rec_sockcall_tcp_recvmsg recvmsg;
		struct derand_rec_sockcall_tcp_close close;
		struct derand_rec_sockcall_tcp_splice_read splice_read;
		struct derand_rec_sockcall_setsockopt setsockopt;
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
		case DERAND_SOCKCALL_TYPE_CLOSE:
			sprintf(buf, "close");
			break;
		case DERAND_SOCKCALL_TYPE_SPLICE_READ:
			sprintf(buf, "splice_read");
			break;
		case DERAND_SOCKCALL_TYPE_SETSOCKOPT:
			sprintf(buf, "setsockopt");
			break;
		default:
			sprintf(buf, "unknown sockcall type %u", sc->type);
	}
	return buf;
}
#define SC_ID_MASK 0x0fffffff
#define get_sockcall_idx(type) (((type) - DERAND_SOCK_ID_BASE) & SC_ID_MASK)

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
	#if GET_EVENT_STAMP
	u64 ts;
	#endif
	#if GET_CWND
	u32 cwnd, ssthresh;
	u32 is_cwnd_limited;
	#endif
};
static inline bool evt_is_sockcall(const struct derand_event* e){
	return e->type >= DERAND_SOCK_ID_BASE;
}
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
	union{
		struct {
			u32 fin_seq; // the seq of fin
			u32 fin; // if this side's fin_seq is set
		};
		u64 fin_v64; // for setting fin_seq and fin atomically
	};
	u8 first; // if the first valid ipid is recorded or not
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

#define DERAND_EFFECT_BOOL_N_LOC 17

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

#if ADVANCED_EVENT_ENABLE
struct AdvancedEvent{
	u8 type, loc, n, fmt;
};
static inline u32 get_ae_hdr_u32(u32 type, u32 loc, u32 n, u32 fmt){
	return type | (loc << 8) | (n << 16) | (fmt << 24);
}
static inline const char* get_ae_name(u8 type, char* buf){
	switch (type){
		case 0: sprintf(buf, "tcp_sendmsg");break;
		case 1: sprintf(buf, "tcp_recvmsg");break;
		case 2: sprintf(buf, "tcp_close");break;
		case 3: sprintf(buf, "tcp_sndbuf_expand");break;
		case 4: sprintf(buf, "tcp_write_xmit");break;
		case 5: sprintf(buf, "tcp_push");break;
		case 6: sprintf(buf, "__tcp_push_pending_frames");break;
		case 7: sprintf(buf, "tcp_push_one");break;
		case 8: sprintf(buf, "tcp_nagle_test");break;
		case 9: sprintf(buf, "tcp_transmit_skb");break;
		case 10: sprintf(buf, "sk_stream_alloc_skb");break;
		case 11: sprintf(buf, "sk_stream_moderate_sndbuf");break;
		case 12: sprintf(buf, "tcp_rcv_established");break;
		case 13: sprintf(buf, "tcp_ack");break;
		case 14: sprintf(buf, "tcp_release_cb");break;
		case 15: sprintf(buf, "__tcp_retransmit_skb");break;
		case 16: sprintf(buf, "tcp_retransmit_skb");break;
		case 17: sprintf(buf, "tcp_xmit_retransmit_queue");break;
		case 18: sprintf(buf, "inet_csk_reset_xmit_timer");break;
		case 19: sprintf(buf, "sk_reset_timer");break;
		case 20: sprintf(buf, "tcp_write_timer");break;
		case 21: sprintf(buf, "tcp_write_timer_handler");break;
		case 22: sprintf(buf, "sk_stream_memory_free");break;
		case 23: sprintf(buf, "sk_stream_write_space");break;
		case 24: sprintf(buf, "tcp_delack_timer");break;
		case 25: sprintf(buf, "tcp_delack_timer_handler");break;
		case 26: sprintf(buf, "tcp_minshall_update");break;
		case 27: sprintf(buf, "tcp_check_space");break;
		case 28: sprintf(buf, "tcp_new_space");break;
		case 29: sprintf(buf, "tcp_should_expand_sndbuf");break;
		case 30: sprintf(buf, "tcp_data_snd_check");break;
		case 31: sprintf(buf, "tcp_fastretrans_alert");break;
		case 32: sprintf(buf, "tcp_time_to_recover");break;
		case 256-1: sprintf(buf, "#read_jiffies");break;
		case 256-2: sprintf(buf, "#under_memory_pressure");break;
		case 256-3: sprintf(buf, "#memory_allocated");break;
		case 256-4: sprintf(buf, "#socket_allocated");break;
		case 256-5: sprintf(buf, "#skb_mstamp");break;
		case 256-6: sprintf(buf, "#effect_bool");break;
		case 256-7: sprintf(buf, "#skb_still_in_host_queue");break;
		default: sprintf(buf, "unknown"); break;
	}
	return buf;
}
#endif

#endif /* _SHARED_DATA_STRUCT__BASE_STRUCT_H */
