#ifndef _SHARED_DATA_STRUCT__TCP_SOCK_INIT_DATA_H
#define _SHARED_DATA_STRUCT__TCP_SOCK_INIT_DATA_H

struct tcp_sock_init_data{
	u16	tcp_header_len;	/* Bytes of tcp header to send		*/
	u32	segs_in;	/* RFC4898 tcpEStatsPerfSegsIn
				 * total number of segments in.
				 */
 	u32	rcv_nxt;	/* What we want to receive next 	*/
	u32	copied_seq;	/* Head of yet unread data		*/
	u32	rcv_wup;	/* rcv_nxt on last window update sent	*/
 	u32	snd_nxt;	/* Next sequence we send		*/
 	u32	snd_una;	/* First byte we want an ack for	*/
 	u32	snd_sml;	/* Last byte of the most recently transmitted small packet */
	u32	lsndtime;	/* timestamp of last sent data packet (for restart window) */
	u32	snd_wl1;	/* Sequence for window update		*/
	u32	snd_wnd;	/* The window we expect to receive	*/
	u32	max_window;	/* Maximal window ever seen from peer	*/
	u32	window_clamp;	/* Maximal window to advertise		*/
	u32	rcv_ssthresh;	/* Current window clamp			*/
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
		long	ts_recent_stamp;/* Time we stored ts_recent (for aging) */
		u32	ts_recent;	/* Time stamp to echo next		*/
		u16 	saw_tstamp : 1,	/* Saw TIMESTAMP on last packet		*/
				tstamp_ok : 1,	/* TIMESTAMP seen on SYN packet		*/
				dsack : 1,	/* D-SACK is scheduled			*/
				wscale_ok : 1,	/* Wscale seen on SYN packet		*/
				sack_ok : 4,	/* SACK seen on SYN packet		*/
				snd_wscale : 4,	/* Window scaling received from sender	*/
				rcv_wscale : 4;	/* Window scaling to send to receiver	*/
		u16	mss_clamp;	/* Maximal mss, negotiated at connection setup */
	} rx_opt;
 	u32	rcv_wnd;	/* Current receiver window		*/
	u32	write_seq;	/* Tail(+1) of data held in tcp send buffer */
	u32	pushed_seq;	/* Last pushed seq, required to talk to windows */
	u32	total_retrans;	/* Total retransmits for entire connection */

	u32			  icsk_rto;
	struct {
		u32		  lrcvtime;	 /* timestamp of last received data packet */
		u16		  last_seg_size; /* Size of last incoming segment	   */
	} icsk_ack;

	unsigned int  sk_userlocks : 4;
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
	• tcp_child_process
		○ tp->segs_in
*/
