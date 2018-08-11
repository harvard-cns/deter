#ifndef _KMOD__COPY_TO_SOCK_INIT_VAL_H
#define _KMOD__COPY_TO_SOCK_INIT_VAL_H

#include <linux/tcp.h>
#include "derand_replayer.h"

static inline void copy_to_sock(struct sock *sk){
	struct inet_sock *isk = inet_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_sock_init_data *d = &((struct derand_replayer*)sk->replayer)->init_data;
	tp->tcp_header_len = d->tcp_header_len;
	tp->pred_flags = d->pred_flags;
	tp->segs_in = d->segs_in;
 	tp->rcv_nxt = d->rcv_nxt;
	tp->copied_seq = d->copied_seq;
	tp->rcv_wup = d->rcv_wup;
 	tp->snd_nxt = d->snd_nxt;
	tp->segs_out = d->segs_out;
	tp->bytes_acked = d->bytes_acked;
 	tp->snd_una = d->snd_una;
 	tp->snd_sml = d->snd_sml;
	tp->rcv_tstamp = d->rcv_tstamp;
	tp->lsndtime = d->lsndtime;
	tp->snd_wl1 = d->snd_wl1;
	tp->snd_wnd = d->snd_wnd;
	tp->max_window = d->max_window;
	tp->window_clamp = d->window_clamp;
	tp->rcv_ssthresh = d->rcv_ssthresh;
	memcpy(&tp->rack, &d->rack, sizeof(d->rack));
	tp->advmss = d->advmss;
	tp->nonagle = d->nonagle;
	tp->thin_lto = d->thin_lto;
	tp->thin_dupack = d->thin_dupack;
	tp->repair = d->repair;
	tp->frto = d->frto;
	tp->do_early_retrans = d->do_early_retrans;
	tp->syn_data = d->syn_data;
	tp->syn_fastopen = d->syn_fastopen;
	tp->syn_fastopen_exp = d->syn_fastopen_exp;
	tp->syn_data_acked = d->syn_data_acked;
	tp->save_syn = d->save_syn;
	tp->is_cwnd_limited = d->is_cwnd_limited;
	tp->srtt_us = d->srtt_us;
	tp->mdev_us = d->mdev_us;
	tp->mdev_max_us = d->mdev_max_us;
	tp->rttvar_us = d->rttvar_us;
	tp->rtt_seq = d->rtt_seq;
	memcpy(tp->rtt_min, d->rtt_min, sizeof(tp->rtt_min));
	tp->ecn_flags = d->ecn_flags;
	tp->snd_up = d->snd_up;
	memcpy(&tp->rx_opt, &d->rx_opt, sizeof(d->rx_opt));
	tp->snd_cwnd = d->snd_cwnd;
	tp->snd_cwnd_stamp = d->snd_cwnd_stamp;
 	tp->rcv_wnd = d->rcv_wnd;
	tp->write_seq = d->write_seq;
	tp->pushed_seq = d->pushed_seq;
	tp->undo_retrans = d->undo_retrans;
	tp->total_retrans = d->total_retrans;
	memcpy(&tp->rcv_rtt_est, &d->rcv_rtt_est, sizeof(d->rcv_rtt_est));
	memcpy(&tp->rcvq_space, &d->rcvq_space, sizeof(d->rcvq_space));

	icsk->icsk_timeout = d->icsk_timeout;
	icsk->icsk_rto = d->icsk_rto;
	memcpy(&icsk->icsk_ack, &d->icsk_ack, sizeof(d->icsk_ack));
	memcpy(&icsk->icsk_mtup, &d->icsk_mtup, sizeof(d->icsk_mtup));

	isk->mc_ttl = d->mc_ttl;
	isk->recverr = d->recverr;
	isk->is_icsk = d->is_icsk;
	isk->freebind = d->freebind;
	isk->hdrincl = d->hdrincl;
	isk->mc_loop = d->mc_loop;
	isk->transparent = d->transparent;
	isk->mc_all = d->mc_all;
	isk->nodefrag = d->nodefrag;
	isk->mc_index = d->mc_index;
	
	sk->sk_family = d->skc_family;
	sk->sk_reuse = d->skc_reuse;
	sk->sk_reuseport = d->skc_reuseport;
	sk->sk_flags = d->skc_flags;
	sk->sk_forward_alloc = d->sk_forward_alloc;
	sk->sk_ll_usec = d->sk_ll_usec;
	sk->sk_rcvbuf = d->sk_rcvbuf;
	sk->sk_sndbuf = d->sk_sndbuf;
	sk->sk_no_check_tx = d->sk_no_check_tx;
	sk->sk_userlocks = d->sk_userlocks;
	sk->sk_pacing_rate = d->sk_pacing_rate;
	sk->sk_max_pacing_rate = d->sk_max_pacing_rate;
	sk->sk_route_caps = d->sk_route_caps;
	sk->sk_route_nocaps = d->sk_route_nocaps;
	sk->sk_rcvlowat = d->sk_rcvlowat;
	sk->sk_lingertime = d->sk_lingertime;
	sk->sk_max_ack_backlog = d->sk_max_ack_backlog;
	sk->sk_priority = d->sk_priority;
	sk->sk_rcvtimeo = d->sk_rcvtimeo;
	sk->sk_sndtimeo = d->sk_sndtimeo;
	sk->sk_tsflags = d->sk_tsflags;
	sk->sk_tskey = d->sk_tskey;
	sk->sk_mark = d->sk_mark;
}

#endif /* _KMOD__COPY_TO_SOCK_INIT_VAL_H */
