#ifndef _SHARED_DATA_STRUCT__TCP_SOCK_INIT_DATA_H
#define _SHARED_DATA_STRUCT__TCP_SOCK_INIT_DATA_H

struct tcp_sock_init_data{
	u16	tcp_header_len;	/* Bytes of tcp header to send		*/
	u32 pred_flags;
	u32	segs_in;	/* RFC4898 tcpEStatsPerfSegsIn
				 * total number of segments in.
				 */
 	u32	rcv_nxt;	/* What we want to receive next 	*/
	u32	copied_seq;	/* Head of yet unread data		*/
	u32	rcv_wup;	/* rcv_nxt on last window update sent	*/
 	u32	snd_nxt;	/* Next sequence we send		*/
	u32	segs_out;	/* RFC4898 tcpEStatsPerfSegsOut
				 * The total number of segments sent.
				 */
	u64	bytes_acked;	/* RFC4898 tcpEStatsAppHCThruOctetsAcked
				 * sum(delta(snd_una)), or how many bytes
				 * were acked.
				 */
 	u32	snd_una;	/* First byte we want an ack for	*/
 	u32	snd_sml;	/* Last byte of the most recently transmitted small packet */
	u32	rcv_tstamp;	/* timestamp of last received ACK (for keepalives) */
	u32	lsndtime;	/* timestamp of last sent data packet (for restart window) */
	u32	snd_wl1;	/* Sequence for window update		*/
	u32	snd_wnd;	/* The window we expect to receive	*/
	u32	max_window;	/* Maximal window ever seen from peer	*/
	u32	window_clamp;	/* Maximal window to advertise		*/
	u32	rcv_ssthresh;	/* Current window clamp			*/
	struct {
		struct skb_mstamp mstamp; /* (Re)sent time of the skb */
		u8 advanced; /* mstamp advanced since last lost marking */
		u8 reord;    /* reordering detected */
	} rack;
	u16	advmss;		/* Advertised MSS			*/
	u8	nonagle     : 4,/* Disable Nagle algorithm?             */
		thin_lto    : 1,/* Use linear timeouts for thin streams */
		thin_dupack : 1,/* Fast retransmit on first dupack      */
		repair      : 1,
		frto        : 1;/* F-RTO (RFC5682) activated in CA_Loss */
	u8	do_early_retrans:1,/* Enable RFC5827 early-retransmit  */
		syn_data:1,	/* SYN includes data */
		syn_fastopen:1,	/* SYN includes Fast Open option */
		syn_fastopen_exp:1,/* SYN includes Fast Open exp. option */
		syn_data_acked:1,/* data in SYN is acked by SYN-ACK */
		save_syn:1,	/* Save headers of SYN packet */
		is_cwnd_limited:1;/* forward progress limited by snd_cwnd? */
/* RTT measurement */
	u32	srtt_us;	/* smoothed round trip time << 3 in usecs */
	u32	mdev_us;	/* medium deviation			*/
	u32	mdev_max_us;	/* maximal mdev for the last rtt period	*/
	u32	rttvar_us;	/* smoothed mdev_max			*/
	u32	rtt_seq;	/* sequence number to update rttvar	*/
	struct {
		u32 rtt, ts;	/* RTT in usec and sampling time in jiffies. */
	} rtt_min[3];
	u8	ecn_flags;	/* ECN status bits.			*/
	u32	snd_up;		/* Urgent pointer		*/
	struct {
		/*	PAWS/RTTM data	*/
		long	ts_recent_stamp;/* Time we stored ts_recent (for aging) */
		u32	ts_recent;	/* Time stamp to echo next		*/
		u32	rcv_tsval;	/* Time stamp value             	*/
		u32	rcv_tsecr;	/* Time stamp echo reply        	*/
		u16 	saw_tstamp : 1,	/* Saw TIMESTAMP on last packet		*/
				tstamp_ok : 1,	/* TIMESTAMP seen on SYN packet		*/
				dsack : 1,	/* D-SACK is scheduled			*/
				wscale_ok : 1,	/* Wscale seen on SYN packet		*/
				sack_ok : 4,	/* SACK seen on SYN packet		*/
				snd_wscale : 4,	/* Window scaling received from sender	*/
				rcv_wscale : 4;	/* Window scaling to send to receiver	*/
		u8	num_sacks;	/* Number of SACK blocks		*/
		u16	user_mss;	/* mss requested by user in ioctl	*/
		u16	mss_clamp;	/* Maximal mss, negotiated at connection setup */
	} rx_opt;
 	u32	snd_cwnd;	/* Sending congestion window		*/
	u32	snd_cwnd_stamp;
 	u32	rcv_wnd;	/* Current receiver window		*/
	u32	write_seq;	/* Tail(+1) of data held in tcp send buffer */
	u32	pushed_seq;	/* Last pushed seq, required to talk to windows */
	int	undo_retrans;	/* number of undoable retransmissions. */
	u32	total_retrans;	/* Total retransmits for entire connection */
/* Receiver side RTT estimation */
	struct {
		u32	rtt;
		u32	seq;
		u32	time;
	} rcv_rtt_est;

/* Receiver queue space */
	struct {
		int	space;
		u32	seq;
		u32	time;
	} rcvq_space;

	unsigned long		  icsk_timeout;
	u32			  icsk_rto;
	struct {
		__u8		  pending;	 /* ACK is pending			   */
		__u8		  quick;	 /* Scheduled number of quick acks	   */
		__u8		  pingpong;	 /* The session is interactive		   */
		__u8		  blocked;	 /* Delayed ACK was blocked by socket lock */
		__u32		  ato;		 /* Predicted tick of soft clock	   */
		unsigned long	  timeout;	 /* Currently scheduled timeout		   */
		__u32		  lrcvtime;	 /* timestamp of last received data packet */
		__u16		  last_seg_size; /* Size of last incoming segment	   */
		__u16		  rcv_mss;	 /* MSS used for delayed ACK decisions	   */ 
	} icsk_ack;
	struct {
		int		  enabled;
		/* Range of MTUs to search */
		int		  search_high;
		int		  search_low;
		/* Information on the current probe. */
		int		  probe_size;
		u32		  probe_timestamp;
	} icsk_mtup;

	unsigned char		skc_reuse:4;
	unsigned char		skc_reuseport:1;
	unsigned long skc_flags; // cannot name sk_flags because sk_flags is defined as a MACRO in sock.h
	int			sk_forward_alloc;
	unsigned int		sk_ll_usec;
	int			sk_rcvbuf;
	int			sk_sndbuf;
	unsigned int  sk_no_check_tx : 1,
				sk_userlocks : 4;
	u32			sk_pacing_rate; /* bytes per second */
	u32			sk_max_pacing_rate;
	u64 sk_route_caps;
	u64 sk_route_nocaps;
	int			sk_rcvlowat;
	unsigned long	        sk_lingertime;
	__u32			sk_priority;
	long			sk_rcvtimeo;
	long			sk_sndtimeo;
	u16			sk_tsflags;
	u32			sk_tskey;
	__u32			sk_mark;
};

#endif /* _SHARED_DATA_STRUCT__TCP_SOCK_INIT_DATA_H */

/*
	• Check tcp_ack_update_rtt()
		○ tp->rtt_min
		○ tp->mdev_us
		○ tp->mdev_max_us
		○ tp->rttvar_us
		○ tp->rtt_seq
		○ tp->srtt_us
		○ inet_csk(sk)->icsk_rto
	• sk_clone_lock
		○ sock_copy(newsk, sk) [guess no]
		○ sk->sk_userlocks
	• inet_csk_clone_lock
		○ sk->sk_mark? [guess no]
		○ sk->sk_cookie? [for sock_diag? guess no]
		○ security_inet_csk_clone(newsk, req)? [security related; guess no]
	• tcp_create_openreq_child
		○ tp->rcv_wup
		○ tp->copied_seq
		○ tp->rcv_nxt
		○ tp->snd_sml
		○ tp->snd_una
		○ tp->snd_nxt
		○ tp->snd_up
		○ tp->snd_wl1
		○ inet_csk(sk)->icsk_ack.lrcvtime
		○ tp->lsndtime
		○ tp->total_retrans
		○ tcp_init_xmit_timers(sk) // need to replace with dumb function
			§ icsk->icsk_retransmit_timer
			§ icsk->icsk_delack_timer
			§ sk->sk_timer
		○ tp->write_seq
		○ tp->pushed_seq
		○ tp->rx_opt.tstamp_ok
		○ tp->rx_opt.sack_ok
		○ tp->window_clamp
		○ tp->rcv_ssthresh
		○ tp->rcv_wnd
		○ tp->rx_opt.wscale_ok
		○ tp->rx_opt.snd_wscale
		○ tp->rx_opt.rcv_wscale
		○ tp->snd_wnd
		○ tp->max_window
		○ tp->rx_opt.ts_recent
		○ tp->rx_opt.ts_recent_stamp
		○ tp->tcp_header_len
		○ icsk->icsk_ack.last_seg_size
		○ tp->rx_opt.mss_clamp
		○ tp->ecn_flags
	• tcp_rcv_state_process
		○ tp->total_retrans
		○ icsk->icsk_mtup
		○ tp->copied_seq
		○ sk->sk_sndbuf
		○ sk->sk_rcvbuf
		○ tp->rcvq_space
		○ tp->window_clamp
		○ tp->rcv_ssthresh
		○ tp->snd_cwnd_stamp
		○ tp->snd_una
		○ tp->snd_wnd
		○ tp->snd_wl1
		○ tp->advmss
		○ sk->sk_pacing_rate
		○ tp->lsndtime
		○ icsk->icsk_ack.rcv_mss
		○ tp->pred_flags
	• tcp_child_process
		○ tp->segs_in
*/
