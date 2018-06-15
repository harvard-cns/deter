#ifndef _COPY_SOCK_INIT_VAL_H
#define _COPY_SOCK_INIT_VAL_H

#include <linux/tcp.h>
#include "derand_recorder.h"

static inline void server_sock_copy(struct sock *sk){
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_sock_init_data *d = &((struct derand_recorder*)sk->recorder)->init_data;
	d->tcp_header_len = tp->tcp_header_len;
	d->segs_in = tp->segs_in;
 	d->rcv_nxt = tp->rcv_nxt;
	d->copied_seq = tp->copied_seq;
	d->rcv_wup = tp->rcv_wup;
 	d->snd_nxt = tp->snd_nxt;
 	d->snd_una = tp->snd_una;
 	d->snd_sml = tp->snd_sml;
	d->lsndtime = tp->lsndtime;
	d->snd_wl1 = tp->snd_wl1;
	d->snd_wnd = tp->snd_wnd;
	d->max_window = tp->max_window;
	d->window_clamp = tp->window_clamp;
	d->rcv_ssthresh = tp->rcv_ssthresh;
	d->srtt_us = tp->srtt_us;
	d->mdev_us = tp->mdev_us;
	d->mdev_max_us = tp->mdev_max_us;
	d->rttvar_us = tp->rttvar_us;
	d->rtt_seq = tp->rtt_seq;
	memcpy(d->rtt_min, tp->rtt_min, sizeof(d->rtt_min));
	d->ecn_flags = tp->ecn_flags;
	d->snd_up = tp->snd_up;
	d->rx_opt.ts_recent_stamp = tp->rx_opt.ts_recent_stamp;
	d->rx_opt.ts_recent = tp->rx_opt.ts_recent;
	d->rx_opt.saw_tstamp = tp->rx_opt.saw_tstamp;
	d->rx_opt.tstamp_ok = tp->rx_opt.tstamp_ok;
	d->rx_opt.dsack = tp->rx_opt.dsack;
	d->rx_opt.wscale_ok = tp->rx_opt.wscale_ok;
	d->rx_opt.sack_ok = tp->rx_opt.sack_ok;
	d->rx_opt.snd_wscale = tp->rx_opt.snd_wscale;
	d->rx_opt.rcv_wscale = tp->rx_opt.rcv_wscale;
	d->rx_opt.mss_clamp = tp->rx_opt.mss_clamp;
	d->snd_cwnd_stamp = tp->snd_cwnd_stamp;
 	d->rcv_wnd = tp->rcv_wnd;
	d->write_seq = tp->write_seq;
	d->pushed_seq = tp->pushed_seq;
	d->total_retrans = tp->total_retrans;

	d->icsk_rto = icsk->icsk_rto;
	d->icsk_ack.lrcvtime = icsk->icsk_ack.lrcvtime;
	d->icsk_ack.last_seg_size = icsk->icsk_ack.last_seg_size;
	
	d->sk_userlocks = sk->sk_userlocks;
}

#endif /* _COPY_SOCK_INIT_VAL_H */
