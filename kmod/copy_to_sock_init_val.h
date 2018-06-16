#ifndef _KMOD__COPY_TO_SOCK_INIT_VAL_H
#define _KMOD__COPY_TO_SOCK_INIT_VAL_H

#include <linux/tcp.h>
#include "derand_replayer.h"

static inline void copy_to_server_sock(struct sock *sk){
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_sock_init_data *d = &((struct derand_replayer*)sk->replayer)->init_data;
	tp->tcp_header_len = d->tcp_header_len;
	tp->segs_in = d->segs_in;
 	tp->rcv_nxt = d->rcv_nxt;
	tp->copied_seq = d->copied_seq;
	tp->rcv_wup = d->rcv_wup;
 	tp->snd_nxt = d->snd_nxt;
 	tp->snd_una = d->snd_una;
 	tp->snd_sml = d->snd_sml;
	tp->lsndtime = d->lsndtime;
	tp->snd_wl1 = d->snd_wl1;
	tp->snd_wnd = d->snd_wnd;
	tp->max_window = d->max_window;
	tp->window_clamp = d->window_clamp;
	tp->rcv_ssthresh = d->rcv_ssthresh;
	tp->srtt_us = d->srtt_us;
	tp->mdev_us = d->mdev_us;
	tp->mdev_max_us = d->mdev_max_us;
	tp->rttvar_us = d->rttvar_us;
	tp->rtt_seq = d->rtt_seq;
	memcpy(tp->rtt_min, d->rtt_min, sizeof(tp->rtt_min));
	tp->ecn_flags = d->ecn_flags;
	tp->snd_up = d->snd_up;
	tp->rx_opt.ts_recent_stamp = d->rx_opt.ts_recent_stamp;
	tp->rx_opt.ts_recent = d->rx_opt.ts_recent;
	tp->rx_opt.saw_tstamp = d->rx_opt.saw_tstamp;
	tp->rx_opt.tstamp_ok = d->rx_opt.tstamp_ok;
	tp->rx_opt.dsack = d->rx_opt.dsack;
	tp->rx_opt.wscale_ok = d->rx_opt.wscale_ok;
	tp->rx_opt.sack_ok = d->rx_opt.sack_ok;
	tp->rx_opt.snd_wscale = d->rx_opt.snd_wscale;
	tp->rx_opt.rcv_wscale = d->rx_opt.rcv_wscale;
	tp->rx_opt.mss_clamp = d->rx_opt.mss_clamp;
	tp->snd_cwnd_stamp = d->snd_cwnd_stamp;
 	tp->rcv_wnd = d->rcv_wnd;
	tp->write_seq = d->write_seq;
	tp->pushed_seq = d->pushed_seq;
	tp->total_retrans = d->total_retrans;

	icsk->icsk_rto = d->icsk_rto;
	icsk->icsk_ack.lrcvtime = d->icsk_ack.lrcvtime;
	icsk->icsk_ack.last_seg_size = d->icsk_ack.last_seg_size;
	
	sk->sk_userlocks = d->sk_userlocks;
}

#endif /* _KMOD__COPY_TO_SOCK_INIT_VAL_H */
