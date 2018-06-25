#ifndef _KMOD__COPY_FROM_SOCK_INIT_VAL_H
#define _KMOD__COPY_FROM_SOCK_INIT_VAL_H

#include <linux/tcp.h>
#include "derand_recorder.h"

static inline void copy_from_server_sock(struct sock *sk){
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_sock_init_data *d = &((struct derand_recorder*)sk->recorder)->init_data;
	d->tcp_header_len = tp->tcp_header_len;
	d->pred_flags = tp->pred_flags;
	d->segs_in = tp->segs_in;
 	d->rcv_nxt = tp->rcv_nxt;
	d->copied_seq = tp->copied_seq;
	d->rcv_wup = tp->rcv_wup;
 	d->snd_nxt = tp->snd_nxt;
	d->segs_out = tp->segs_out;
	d->bytes_acked = tp->bytes_acked;
 	d->snd_una = tp->snd_una;
 	d->snd_sml = tp->snd_sml;
	d->rcv_tstamp = tp->rcv_tstamp;
	d->lsndtime = tp->lsndtime;
	d->snd_wl1 = tp->snd_wl1;
	d->snd_wnd = tp->snd_wnd;
	d->max_window = tp->max_window;
	d->window_clamp = tp->window_clamp;
	d->rcv_ssthresh = tp->rcv_ssthresh;
	memcpy(&d->rack, &tp->rack, sizeof(d->rack));
	d->advmss = tp->advmss;
	d->srtt_us = tp->srtt_us;
	d->mdev_us = tp->mdev_us;
	d->mdev_max_us = tp->mdev_max_us;
	d->rttvar_us = tp->rttvar_us;
	d->rtt_seq = tp->rtt_seq;
	memcpy(d->rtt_min, tp->rtt_min, sizeof(d->rtt_min));
	d->ecn_flags = tp->ecn_flags;
	d->snd_up = tp->snd_up;
	memcpy(&d->rx_opt, &tp->rx_opt, sizeof(d->rx_opt));
	d->snd_cwnd = tp->snd_cwnd;
	d->snd_cwnd_stamp = tp->snd_cwnd_stamp;
 	d->rcv_wnd = tp->rcv_wnd;
	d->write_seq = tp->write_seq;
	d->pushed_seq = tp->pushed_seq;
	d->undo_retrans = tp->undo_retrans;
	d->total_retrans = tp->total_retrans;
	memcpy(&d->rcv_rtt_est, &tp->rcv_rtt_est, sizeof(d->rcv_rtt_est));
	memcpy(&d->rcvq_space, &tp->rcvq_space, sizeof(d->rcvq_space));

	d->icsk_timeout = icsk->icsk_timeout;
	d->icsk_rto = icsk->icsk_rto;
	memcpy(&d->icsk_ack, &icsk->icsk_ack, sizeof(d->icsk_ack));
	memcpy(&d->icsk_mtup, &icsk->icsk_mtup, sizeof(d->icsk_mtup));
	
	d->skc_reuse = sk->sk_reuse;
	d->skc_reuseport = sk->sk_reuseport;
	d->skc_flags = sk->sk_flags;
	d->sk_forward_alloc = sk->sk_forward_alloc;
	d->sk_ll_usec = sk->sk_ll_usec;
	d->sk_rcvbuf = sk->sk_rcvbuf;
	d->sk_sndbuf = sk->sk_sndbuf;
	d->sk_no_check_tx = sk->sk_no_check_tx;
	d->sk_userlocks = sk->sk_userlocks;
	d->sk_pacing_rate = sk->sk_pacing_rate;
	d->sk_max_pacing_rate = sk->sk_max_pacing_rate;
	d->sk_route_caps = sk->sk_route_caps;
	d->sk_route_nocaps = sk->sk_route_nocaps;
	d->sk_rcvlowat = sk->sk_rcvlowat;
	d->sk_lingertime = sk->sk_lingertime;
	d->sk_priority = sk->sk_priority;
	d->sk_rcvtimeo = sk->sk_rcvtimeo;
	d->sk_sndtimeo = sk->sk_sndtimeo;
	d->sk_tsflags = sk->sk_tsflags;
	d->sk_tskey = sk->sk_tskey;
	d->sk_mark = sk->sk_mark;

	/* print */
	#if 0
	int i = 0;
	struct inet_sock *inetsk = inet_sk(sk);
	printk("sk->sk_refcnt=%d\n", sk->sk_refcnt.counter);
	printk("sk->sk_tx_queue_mapping=%d\n", sk->sk_tx_queue_mapping);
	printk("sk->sk_hash=%u\n", sk->sk_hash);
	printk("sk->sk_num=%hu\n", sk->sk_num);
	printk("sk->sk_dport=%hu\n", sk->sk_dport);
	printk("sk->sk_daddr=%u\n", sk->sk_daddr);
	printk("sk->sk_rcv_saddr=%u\n", sk->sk_rcv_saddr);
	printk("sk->sk_family=%hu\n", sk->sk_family);
	printk("sk->sk_state=%hhu\n", sk->sk_state);
	printk("sk->sk_reuse=%hhu\n", sk->sk_reuse);
	printk("sk->sk_reuseport=%hhu\n", sk->sk_reuseport);
	printk("sk->sk_ipv6only=%hhu\n", sk->sk_ipv6only);
	printk("sk->sk_net_refcnt=%hhu\n", sk->sk_net_refcnt);
	printk("sk->sk_bound_dev_if=%d\n", sk->sk_bound_dev_if);
	printk("sk->sk_flags=%lu\n", sk->sk_flags);
	printk("sk->sk_rxhash=%u\n", sk->sk_rxhash);
	printk("sk->sk_rmem_alloc=%d\n", sk->sk_rmem_alloc.counter);
	printk("sk->sk_backlog.len=%d\n", sk->sk_backlog.len);
	printk("sk->sk_forward_alloc=%d\n", sk->sk_forward_alloc);
	printk("sk->sk_txhash=%u\n", sk->sk_txhash);
	printk("sk->sk_napi_id=%u\n", sk->sk_napi_id);
	printk("sk->sk_ll_usec=%u\n", sk->sk_ll_usec);
	printk("sk->sk_drops.counter=%d\n", sk->sk_drops.counter);
	printk("sk->sk_rcvbuf=%d\n", sk->sk_rcvbuf);
	printk("sk->sk_wmem_alloc.counter=%d\n", sk->sk_wmem_alloc.counter);
	printk("sk->sk_omem_alloc.counter=%d\n", sk->sk_omem_alloc.counter);
	printk("sk->sk_sndbuf=%d\n", sk->sk_sndbuf);
	printk("sk->sk_dst_pending_confirm=%u\n", sk->sk_dst_pending_confirm);
	printk("sk->sk_shutdown=%u\n", sk->sk_shutdown);
	printk("sk->sk_no_check_tx=%u\n", sk->sk_no_check_tx);
	printk("sk->sk_no_check_rx=%u\n", sk->sk_no_check_rx);
	printk("sk->sk_userlocks=%u\n", sk->sk_userlocks);
	printk("sk->sk_protocol=%u\n", sk->sk_protocol);
	printk("sk->sk_type=%u\n", sk->sk_type);
	printk("sk->sk_wmem_queued=%d\n", sk->sk_wmem_queued);
	printk("sk->sk_allocation=%u\n", sk->sk_allocation);
	printk("sk->sk_pacing_rate=%u\n", sk->sk_pacing_rate); 
	printk("sk->sk_max_pacing_rate=%u\n", sk->sk_max_pacing_rate);
	printk("sk->sk_route_caps=%llu\n", sk->sk_route_caps);
	printk("sk->sk_route_nocaps=%llu\n", sk->sk_route_nocaps);
	printk("sk->sk_gso_type=%d\n", sk->sk_gso_type);
	printk("sk->sk_gso_max_size=%u\n", sk->sk_gso_max_size);
	printk("sk->sk_gso_max_segs=%hu\n", sk->sk_gso_max_segs);
	printk("sk->sk_rcvlowat=%d\n", sk->sk_rcvlowat);
	printk("sk->sk_lingertime=%lu\n", sk->sk_lingertime);
	printk("sk->sk_err=%d\n", sk->sk_err);
	printk("sk->sk_err_soft=%d\n", sk->sk_err_soft);
	printk("sk->sk_ack_backlog=%u\n", sk->sk_ack_backlog);
	printk("sk->sk_max_ack_backlog=%u\n", sk->sk_max_ack_backlog);
	printk("sk->sk_priority=%u\n", sk->sk_priority);
	printk("sk->sk_cgrp_prioidx=%u\n", sk->sk_cgrp_prioidx);
	printk("sk->sk_rcvtimeo=%ld\n", sk->sk_rcvtimeo);
	printk("sk->sk_sndtimeo=%ld\n", sk->sk_sndtimeo);
	printk("sk->sk_stamp.tv64=%lld\n", sk->sk_stamp.tv64);
	printk("sk->sk_tsflags=%hu\n", sk->sk_tsflags);
	printk("sk->sk_tskey=%u\n", sk->sk_tskey);
	printk("sk->sk_frag.page=%llx\n", (u64)sk->sk_frag.page);
	printk("sk->sk_frag.offset=%u\n", sk->sk_frag.offset);
	printk("sk->sk_frag.size=%u\n", sk->sk_frag.size);
	printk("sk->sk_peek_off=%d\n", sk->sk_peek_off);
	printk("sk->sk_write_pending=%d\n", sk->sk_write_pending);
	printk("sk->sk_mark=%u\n", sk->sk_mark);
	printk("sk->sk_classid=%u\n", sk->sk_classid);
	printk("sk->sk_cgrp=%llx\n", (u64)sk->sk_cgrp);

	// inet_sock
	printk("inetsk->inet_saddr=%u\n", inetsk->inet_saddr);
	printk("inetsk->uc_ttl=%hd\n", inetsk->uc_ttl);
	printk("inetsk->cmsg_flags=%hu\n", inetsk->cmsg_flags);
	printk("inetsk->inet_sport=%hu\n", inetsk->inet_sport);
	printk("inetsk->inet_id=%hu\n", inetsk->inet_id);
	printk("inetsk->rx_dst_ifindex=%d\n", inetsk->rx_dst_ifindex);
	printk("inetsk->tos=%hhu\n", inetsk->tos);
	printk("inetsk->min_ttl=%hhu\n", inetsk->min_ttl);
	printk("inetsk->mc_ttl=%hhu\n", inetsk->mc_ttl);
	printk("inetsk->pmtudisc=%hhu\n", inetsk->pmtudisc);
	printk("inetsk->recverr=%hhu\n", inetsk->recverr);
	printk("inetsk->is_icsk=%hhu\n", inetsk->is_icsk);
	printk("inetsk->freebind=%hhu\n", inetsk->freebind);
	printk("inetsk->hdrincl=%hhu\n", inetsk->hdrincl);
	printk("inetsk->mc_loop=%hhu\n", inetsk->mc_loop);
	printk("inetsk->transparent=%hhu\n", inetsk->transparent);
	printk("inetsk->mc_all=%hhu\n", inetsk->mc_all);
	printk("inetsk->nodefrag=%hhu\n", inetsk->nodefrag);
	printk("inetsk->bind_address_no_port=%hhu\n", inetsk->bind_address_no_port);
	printk("inetsk->rcv_tos=%hhu\n", inetsk->rcv_tos);
	printk("inetsk->convert_csum=%hhu\n", inetsk->convert_csum);
	printk("inetsk->uc_index=%d\n", inetsk->uc_index);
	printk("inetsk->mc_index=%d\n", inetsk->mc_index);
	printk("inetsk->mc_addr=%u\n", inetsk->mc_addr);
	printk("inetsk->cork.base.flags=%u\n", inetsk->cork.base.flags);
	printk("inetsk->cork.base.addr=%u\n", inetsk->cork.base.addr);
	printk("inetsk->cork.base.opt=%llx\n", (u64)inetsk->cork.base.opt);
	printk("inetsk->cork.base.fragsize=%u\n", inetsk->cork.base.fragsize);
	printk("inetsk->cork.base.length=%d\n", inetsk->cork.base.length); 
	printk("inetsk->cork.base.dst=%llx\n", (u64)inetsk->cork.base.dst);
	printk("inetsk->cork.base.tx_flags=%hhu\n", inetsk->cork.base.tx_flags);
	printk("inetsk->cork.base.ttl=%hhu\n", inetsk->cork.base.ttl);
	printk("inetsk->cork.base.tos=%hd\n", inetsk->cork.base.tos);
	printk("inetsk->cork.base.priority=%hhu\n", inetsk->cork.base.priority);

	// inet_connection_sock
	printk("icsk->icsk_bind_hash=%llx\n", (u64)icsk->icsk_bind_hash);
	printk("icsk->icsk_timeout=%lu\n", icsk->icsk_timeout);
	printk("icsk->icsk_rto=%u\n", icsk->icsk_rto);
	printk("icsk->icsk_pmtu_cookie=%u\n", icsk->icsk_pmtu_cookie);
	printk("icsk->icsk_ca_state=%hhu\n", icsk->icsk_ca_state);
	printk("icsk->icsk_ca_setsockopt=%hhu\n", icsk->icsk_ca_setsockopt);
	printk("icsk->icsk_ca_dst_locked=%hhu\n", icsk->icsk_ca_dst_locked);
	printk("icsk->icsk_retransmits=%hhu\n", icsk->icsk_retransmits);
	printk("icsk->icsk_pending=%hhu\n", icsk->icsk_pending);
	printk("icsk->icsk_backoff=%hhu\n", icsk->icsk_backoff);
	printk("icsk->icsk_syn_retries=%hhu\n", icsk->icsk_syn_retries);
	printk("icsk->icsk_probes_out=%hhu\n", icsk->icsk_probes_out);
	printk("icsk->icsk_ext_hdr_len=%hu\n", icsk->icsk_ext_hdr_len);
	printk("icsk->icsk_ack.pending=%hhu\n", icsk->icsk_ack.pending);	 
	printk("icsk->icsk_ack.quick=%hhu\n", icsk->icsk_ack.quick);	 
	printk("icsk->icsk_ack.pingpong=%hhu\n", icsk->icsk_ack.pingpong);	 
	printk("icsk->icsk_ack.blocked=%hhu\n", icsk->icsk_ack.blocked);	 
	printk("icsk->icsk_ack.ato=%u\n", icsk->icsk_ack.ato);		 
	printk("icsk->icsk_ack.timeout=%lu\n", icsk->icsk_ack.timeout);	 
	printk("icsk->icsk_ack.lrcvtime=%u\n", icsk->icsk_ack.lrcvtime);	 
	printk("icsk->icsk_ack.last_seg_size=%hu\n", icsk->icsk_ack.last_seg_size); 
	printk("icsk->icsk_ack.rcv_mss=%hu\n", icsk->icsk_ack.rcv_mss);	  
	printk("icsk->icsk_mtup.enabled=%d\n", icsk->icsk_mtup.enabled);
	printk("icsk->icsk_mtup.search_high=%d\n", icsk->icsk_mtup.search_high);
	printk("icsk->icsk_mtup.search_low=%d\n", icsk->icsk_mtup.search_low);
	printk("icsk->icsk_mtup.probe_size=%d\n", icsk->icsk_mtup.probe_size);
	printk("icsk->icsk_mtup.probe_timestamp=%u\n", icsk->icsk_mtup.probe_timestamp);
	printk("icsk->icsk_user_timeout=%u\n", icsk->icsk_user_timeout);
	for (i = 0; i < 64 / sizeof(u64); i++)
		printk("icsk->icsk_ca_priv[%d]=%llu\n", i, icsk->icsk_ca_priv[i]);

	// tcp_sock
	printk("tp->tcp_header_len=%hu\n", tp->tcp_header_len);	
	printk("tp->gso_segs=%hu\n", tp->gso_segs);	
	printk("tp->pred_flags=%u\n", tp->pred_flags);
	printk("tp->bytes_received=%llu\n", tp->bytes_received);	
	printk("tp->segs_in=%u\n", tp->segs_in);	
 	printk("tp->rcv_nxt=%u\n", tp->rcv_nxt);	
	printk("tp->copied_seq=%u\n", tp->copied_seq);	
	printk("tp->rcv_wup=%u\n", tp->rcv_wup);	
 	printk("tp->snd_nxt=%u\n", tp->snd_nxt);	
	printk("tp->segs_out=%u\n", tp->segs_out);	
	printk("tp->bytes_acked=%llu\n", tp->bytes_acked);	
	printk("tp->syncp=%llu\n", *(u64*)&tp->syncp);
 	printk("tp->snd_una=%u\n", tp->snd_una);	
 	printk("tp->snd_sml=%u\n", tp->snd_sml);	
	printk("tp->rcv_tstamp=%u\n", tp->rcv_tstamp);	
	printk("tp->lsndtime=%u\n", tp->lsndtime);	
	printk("tp->last_oow_ack_time=%u\n", tp->last_oow_ack_time);  
	printk("tp->tsoffset=%u\n", tp->tsoffset);	
	printk("tp->tsq_flags=%lu\n", tp->tsq_flags);
	printk("tp->ucopy.memory=%d\n", tp->ucopy.memory);
	printk("tp->ucopy.len=%d\n", tp->ucopy.len);
	printk("tp->snd_wl1=%u\n", tp->snd_wl1);	
	printk("tp->snd_wnd=%u\n", tp->snd_wnd);	
	printk("tp->max_window=%u\n", tp->max_window);	
	printk("tp->mss_cache=%u\n", tp->mss_cache);	
	printk("tp->window_clamp=%u\n", tp->window_clamp);	
	printk("tp->rcv_ssthresh=%u\n", tp->rcv_ssthresh);	
	printk("tp->rack.mstamp={%u, %u}\n", tp->rack.mstamp.stamp_us, tp->rack.mstamp.stamp_jiffies);
	printk("tp->rack.advanced=%hhu\n", tp->rack.advanced); 
	printk("tp->rack.reord=%hhu\n", tp->rack.reord);    
	printk("tp->advmss=%hu\n", tp->advmss);		
	printk("tp->unused=%hhu\n", tp->unused);
	printk("tp->nonagle=%hhu\n", tp->nonagle);
	printk("tp->thin_lto=%hhu\n", tp->thin_lto);
	printk("tp->thin_dupack=%hhu\n", tp->thin_dupack);
	printk("tp->repair=%hhu\n", tp->repair);
	printk("tp->frto=%hhu\n", tp->frto);
	printk("tp->repair_queue=%hhu\n", tp->repair_queue);
	printk("tp->do_early_retrans=%hhu\n", tp->do_early_retrans);
	printk("tp->syn_data=%hhu\n", tp->syn_data);
	printk("tp->syn_fastopen=%hhu\n", tp->syn_fastopen);
	printk("tp->syn_fastopen_exp=%hhu\n", tp->syn_fastopen_exp);
	printk("tp->syn_data_acked=%hhu\n", tp->syn_data_acked);
	printk("tp->save_syn=%hhu\n", tp->save_syn);
	printk("tp->is_cwnd_limited=%hhu\n", tp->is_cwnd_limited);
	printk("tp->tlp_high_seq=%u\n", tp->tlp_high_seq);	
	printk("tp->srtt_us=%u\n", tp->srtt_us);	
	printk("tp->mdev_us=%u\n", tp->mdev_us);	
	printk("tp->mdev_max_us=%u\n", tp->mdev_max_us);	
	printk("tp->rttvar_us=%u\n", tp->rttvar_us);	
	printk("tp->rtt_seq=%u\n", tp->rtt_seq);	
	printk("tp->packets_out=%u\n", tp->packets_out);	
	printk("tp->retrans_out=%u\n", tp->retrans_out);	
	printk("tp->max_packets_out=%u\n", tp->max_packets_out);  
	printk("tp->max_packets_seq=%u\n", tp->max_packets_seq);  
	printk("tp->urg_data=%hu\n", tp->urg_data);	
	printk("tp->ecn_flags=%hhu\n", tp->ecn_flags);	
	printk("tp->keepalive_probes=%hhu\n", tp->keepalive_probes); 
	printk("tp->reordering=%u\n", tp->reordering);	
	printk("tp->snd_up=%u\n", tp->snd_up);		
	printk("tp->rx_opt.ts_recent_stamp=%ld\n", tp->rx_opt.ts_recent_stamp);
	printk("tp->rx_opt.ts_recent=%u\n", tp->rx_opt.ts_recent);	
	printk("tp->rx_opt.rcv_tsval=%u\n", tp->rx_opt.rcv_tsval);	
	printk("tp->rx_opt.rcv_tsecr=%u\n", tp->rx_opt.rcv_tsecr);	
	printk("tp->rx_opt.saw_tstamp=%hu\n", tp->rx_opt.saw_tstamp);
	printk("tp->rx_opt.tstamp_ok=%hu\n", tp->rx_opt.tstamp_ok);
	printk("tp->rx_opt.dsack=%hu\n", tp->rx_opt.dsack);
	printk("tp->rx_opt.wscale_ok=%hu\n", tp->rx_opt.wscale_ok);
	printk("tp->rx_opt.sack_ok=%hu\n", tp->rx_opt.sack_ok);
	printk("tp->rx_opt.snd_wscale=%hu\n", tp->rx_opt.snd_wscale);
	printk("tp->rx_opt.rcv_wscale=%hu\n", tp->rx_opt.rcv_wscale);
	printk("tp->rx_opt.num_sacks=%hhu\n", tp->rx_opt.num_sacks);	
	printk("tp->rx_opt.user_mss=%hu\n", tp->rx_opt.user_mss);	
	printk("tp->rx_opt.mss_clamp=%hu\n", tp->rx_opt.mss_clamp);	
 	printk("tp->snd_ssthresh=%u\n", tp->snd_ssthresh);	
 	printk("tp->snd_cwnd=%u\n", tp->snd_cwnd);	
	printk("tp->snd_cwnd_cnt=%u\n", tp->snd_cwnd_cnt);	
	printk("tp->snd_cwnd_clamp=%u\n", tp->snd_cwnd_clamp); 
	printk("tp->snd_cwnd_used=%u\n", tp->snd_cwnd_used);
	printk("tp->snd_cwnd_stamp=%u\n", tp->snd_cwnd_stamp);
	printk("tp->prior_cwnd=%u\n", tp->prior_cwnd);	
	printk("tp->prr_delivered=%u\n", tp->prr_delivered);	
	printk("tp->prr_out=%u\n", tp->prr_out);	
 	printk("tp->rcv_wnd=%u\n", tp->rcv_wnd);	
	printk("tp->write_seq=%u\n", tp->write_seq);	
	printk("tp->notsent_lowat=%u\n", tp->notsent_lowat);	
	printk("tp->pushed_seq=%u\n", tp->pushed_seq);	
	printk("tp->lost_out=%u\n", tp->lost_out);	
	printk("tp->sacked_out=%u\n", tp->sacked_out);	
	printk("tp->fackets_out=%u\n", tp->fackets_out);	
	printk("tp->duplicate_sack[0]={%u,%u}\n", tp->duplicate_sack[0].start_seq, tp->duplicate_sack[0].end_seq);
	printk("tp->selective_acks[0]={%u,%u}\n", tp->selective_acks[0].start_seq, tp->selective_acks[0].end_seq);
	printk("tp->selective_acks[1]={%u,%u}\n", tp->selective_acks[1].start_seq, tp->selective_acks[1].end_seq);
	printk("tp->selective_acks[2]={%u,%u}\n", tp->selective_acks[2].start_seq, tp->selective_acks[2].end_seq);
	printk("tp->selective_acks[3]={%u,%u}\n", tp->selective_acks[3].start_seq, tp->selective_acks[3].end_seq);
	printk("tp->recv_sack_cache[0]={%u,%u}\n", tp->recv_sack_cache[0].start_seq, tp->recv_sack_cache[0].end_seq);
	printk("tp->recv_sack_cache[1]={%u,%u}\n", tp->recv_sack_cache[1].start_seq, tp->recv_sack_cache[1].end_seq);
	printk("tp->recv_sack_cache[2]={%u,%u}\n", tp->recv_sack_cache[2].start_seq, tp->recv_sack_cache[2].end_seq);
	printk("tp->recv_sack_cache[3]={%u,%u}\n", tp->recv_sack_cache[3].start_seq, tp->recv_sack_cache[3].end_seq);
	printk("tp->highest_sack = %llx\n", (u64)tp->highest_sack);
	printk("tp->lost_cnt_hint=%d\n", tp->lost_cnt_hint);
	printk("tp->retransmit_high=%u\n", tp->retransmit_high);	
	printk("tp->prior_ssthresh=%u\n", tp->prior_ssthresh); 
	printk("tp->high_seq=%u\n", tp->high_seq);	
	printk("tp->retrans_stamp=%u\n", tp->retrans_stamp);	
	printk("tp->undo_marker=%u\n", tp->undo_marker);	
	printk("tp->undo_retrans=%d\n", tp->undo_retrans);	
	printk("tp->total_retrans=%u\n", tp->total_retrans);	
	printk("tp->urg_seq=%u\n", tp->urg_seq);	
	printk("tp->keepalive_time=%u\n", tp->keepalive_time);	  
	printk("tp->keepalive_intvl=%u\n", tp->keepalive_intvl);  
	printk("tp->linger2=%d\n", tp->linger2);
	printk("tp->rcv_rtt_est.rtt=%u\n", tp->rcv_rtt_est.rtt);
	printk("tp->rcv_rtt_est.seq=%u\n", tp->rcv_rtt_est.seq);
	printk("tp->rcv_rtt_est.time=%u\n", tp->rcv_rtt_est.time);
	printk("tp->rcvq_space.space=%d\n", tp->rcvq_space.space);
	printk("tp->rcvq_space.seq=%u\n", tp->rcvq_space.seq);
	printk("tp->rcvq_space.time=%u\n", tp->rcvq_space.time);
	printk("tp->mtu_probe.probe_seq_start=%u\n", tp->mtu_probe.probe_seq_start);
	printk("tp->mtu_probe.probe_seq_end=%u\n", tp->mtu_probe.probe_seq_end);
	printk("tp->mtu_info=%u\n", tp->mtu_info); 
	printk("tp->save_syn=%llu\n", (u64)tp->saved_syn);
	#endif
}

static inline void copy_from_client_sock(struct sock *sk){
	copy_from_server_sock(sk);
}

#endif /* _KMOD__COPY_FROM_SOCK_INIT_VAL_H */
