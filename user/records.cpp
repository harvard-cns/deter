#include <cstring>
#include "records.hpp"

using namespace std;

int Records::dump(const char* filename){
	int ret;
	uint32_t len;
	FILE* fout;
	if (filename == NULL){
		char buf[128];
		sprintf(buf, "%08x:%hu->%08x:%hu", sip, sport, dip, dport);
		fout = fopen(buf, "w");
	}else 
		fout = fopen(filename, "w");

	// write 4 tuples
	if (!fwrite(&sip, sizeof(sip)+sizeof(dip)+sizeof(sport)+sizeof(dport), 1, fout))
		goto fail_write;

	// write init_data
	if (!fwrite(&init_data, sizeof(init_data), 1, fout))
		goto fail_write;

	// write events
	len = evts.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&evts[0], sizeof(derand_event) * len, 1, fout))
		goto fail_write;

	// write sockcalls
	len = sockcalls.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&sockcalls[0], sizeof(derand_rec_sockcall) * len, 1, fout))
		goto fail_write;

	// write jiffies_reads
	len = jiffies.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&jiffies[0], sizeof(jiffies_rec) * len, 1, fout))
		goto fail_write;

	// write memory_pressures
	len = memory_pressures.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&memory_pressures[0], sizeof(uint32_t) * len, 1, fout))
		goto fail_write;

	// write n_memory_allocated;
	if (!fwrite(&n_memory_allocated, sizeof(n_memory_allocated), 1, fout))
		goto fail_write;

	// write n_sockets_allocated
	if (!fwrite(&n_sockets_allocated, sizeof(n_sockets_allocated), 1, fout))
		goto fail_write;

	// write mstamp
	if (!fwrite(mstamp, sizeof(mstamp), 1, fout))
		goto fail_write;

	// write effect_bool
	if (!fwrite(effect_bool, sizeof(effect_bool), 1, fout))
		goto fail_write;

	fclose(fout);
	return 0;
fail_write:
	fclose(fout);
	return -1;
}

int Records::read(const char* filename){
	FILE* fin= fopen(filename, "r");
	uint32_t len;
	// read 4 tuples
	if (!fread(&sip, sizeof(sip)+sizeof(dip)+sizeof(sport)+sizeof(dport), 1, fin))
		goto fail_read;

	// read init_data
	if (!fread(&init_data, sizeof(init_data), 1, fin))
		goto fail_read;

	// read events
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	evts.resize(len);
	if (!fread(&evts[0], sizeof(derand_event) * len, 1, fin))
		goto fail_read;

	// read sockcalls
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	sockcalls.resize(len);
	if (!fread(&sockcalls[0], sizeof(derand_rec_sockcall) * len, 1, fin))
		goto fail_read;

	// read jiffies_reads
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	jiffies.resize(len);
	if (!fread(&jiffies[0], sizeof(jiffies_rec) * len, 1, fin))
		goto fail_read;

	// read memory_pressures
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	memory_pressures.resize(len);
	if (!fread(&memory_pressures[0], sizeof(uint32_t) * len, 1, fin))
		goto fail_read;

	// read n_memory_allocated
	if (!fread(&n_memory_allocated, sizeof(n_memory_allocated), 1, fin))
		goto fail_read;
	
	// read n_sockets_allocated
	if (!fread(&n_sockets_allocated, sizeof(n_sockets_allocated), 1, fin))
		goto fail_read;

	// read mstamp
	if (!fread(mstamp, sizeof(mstamp), 1, fin))
		goto fail_read;

	// read effect_bool
	if (!fread(effect_bool, sizeof(effect_bool), 1, fin))
		goto fail_read;

	fclose(fin);
	return 0;
fail_read:
	fclose(fin);
	return -1;
}

void Records::print(FILE* fout){
	fprintf(fout, "%08x:%hu %08x:%hu\n", sip, sport, dip, dport);
	fprintf(fout, "%lu sockcalls\n", sockcalls.size());
	for (int i = 0; i < sockcalls.size(); i++){
		derand_rec_sockcall &sc = sockcalls[i];
		if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
			fprintf(fout, "sendmsg: 0x%x %lu\n", sc.sendmsg.flags, sc.sendmsg.size);
		}else 
			fprintf(fout, "recvmsg: 0x%x %lu\n", sc.recvmsg.flags, sc.recvmsg.size);
	}
	fprintf(fout, "%lu events\n", evts.size());
	for (int i = 0; i < evts.size(); i++){
		derand_event &e = evts[i];
		fprintf(fout, "%u %u\n", e.seq, e.type);
	}
	fprintf(fout, "%lu new jiffies\n", jiffies.size());
	if (jiffies.size() > 0){
		fprintf(fout, "first: %lu\n", jiffies[0].init_jiffies);
		for (int i = 1; i < jiffies.size(); i++)
			fprintf(fout, "%u %u\n", jiffies[i].idx_delta, jiffies[i].jiffies_delta);
	}

	fprintf(fout, "%lu memory_pressures read\n", memory_pressures.size());
	for (int i = 0; i < memory_pressures.size(); i++)
		fprintf(fout, "%08x\n", memory_pressures[i]);

	fprintf(fout, "%u reads to memory_allocated\n", n_memory_allocated);

	fprintf(fout, "%u reads to n_sockets_allocated\n", n_sockets_allocated);

	fprintf(fout, "skb_mstamp_get:\n");
	for (int i = 0; i < 16; i++)
		fprintf(fout, "%d %u\n", i, mstamp[i]);

	fprintf(fout, "effect_bool:\n");
	for (int i = 0; i < 16; i++)
		fprintf(fout, "%d %u\n", i, effect_bool[i]);
}

void Records::print_init_data(FILE* fout){
	tcp_sock_init_data *d = &init_data;
	fprintf(fout, "tcp_header_len = %u\n", d->tcp_header_len);
	fprintf(fout, "segs_in = %u\n", d->segs_in);
	fprintf(fout, "rcv_nxt = %u\n", d->rcv_nxt);
	fprintf(fout, "copied_seq = %u\n", d->copied_seq);
	fprintf(fout, "rcv_wup = %u\n", d->rcv_wup);
	fprintf(fout, "snd_nxt = %u\n", d->snd_nxt);
	fprintf(fout, "snd_una = %u\n", d->snd_una);
	fprintf(fout, "snd_sml = %u\n", d->snd_sml);
	fprintf(fout, "lsndtime = %u\n", d->lsndtime);
	fprintf(fout, "snd_wl1 = %u\n", d->snd_wl1);
	fprintf(fout, "snd_wnd = %u\n", d->snd_wnd);
	fprintf(fout, "max_window = %u\n", d->max_window);
	fprintf(fout, "window_clamp = %u\n", d->window_clamp);
	fprintf(fout, "rcv_ssthresh = %u\n", d->rcv_ssthresh);
	fprintf(fout, "srtt_us = %u\n", d->srtt_us);
	fprintf(fout, "mdev_us = %u\n", d->mdev_us);
	fprintf(fout, "mdev_max_us = %u\n", d->mdev_max_us);
	fprintf(fout, "rttvar_us = %u\n", d->rttvar_us);
	fprintf(fout, "rtt_seq = %u\n", d->rtt_seq);
	fprintf(fout, "rtt_min[0] = {%u,%u}\n", d->rtt_min[0].rtt, d->rtt_min[0].ts);
	fprintf(fout, "rtt_min[1] = {%u,%u}\n", d->rtt_min[1].rtt, d->rtt_min[1].ts);
	fprintf(fout, "rtt_min[2] = {%u,%u}\n", d->rtt_min[2].rtt, d->rtt_min[2].ts);
	fprintf(fout, "ecn_flags = %u\n", d->ecn_flags);
	fprintf(fout, "snd_up = %u\n", d->snd_up);
	fprintf(fout, "rx_opt.ts_recent_stamp = %ld\n", d->rx_opt.ts_recent_stamp);
	fprintf(fout, "rx_opt.ts_recent = %u\n", d->rx_opt.ts_recent);
	fprintf(fout, "rx_opt.saw_tstamp = %u\n", d->rx_opt.saw_tstamp);
	fprintf(fout, "rx_opt.tstamp_ok = %u\n", d->rx_opt.tstamp_ok);
	fprintf(fout, "rx_opt.dsack = %u\n", d->rx_opt.dsack);
	fprintf(fout, "rx_opt.wscale_ok = %u\n", d->rx_opt.wscale_ok);
	fprintf(fout, "rx_opt.sack_ok = %u\n", d->rx_opt.sack_ok);
	fprintf(fout, "rx_opt.snd_wscale = %u\n", d->rx_opt.snd_wscale);
	fprintf(fout, "rx_opt.rcv_wscale = %u\n", d->rx_opt.rcv_wscale);
	fprintf(fout, "rx_opt.mss_clamp = %u\n", d->rx_opt.mss_clamp);
	fprintf(fout, "snd_cwnd_stamp = %u\n", d->snd_cwnd_stamp);
	fprintf(fout, "rcv_wnd = %u\n", d->rcv_wnd);
	fprintf(fout, "write_seq = %u\n", d->write_seq);
	fprintf(fout, "pushed_seq = %u\n", d->pushed_seq);
	fprintf(fout, "total_retrans = %u\n", d->total_retrans);
	fprintf(fout, "icsk_rto = %u\n", d->icsk_rto);
	fprintf(fout, "icsk_ack.lrcvtime = %u\n", d->icsk_ack.lrcvtime);
	fprintf(fout, "icsk_ack.last_seg_size = %u\n", d->icsk_ack.last_seg_size);
	fprintf(fout, "sk_userlocks = %u\n", d->sk_userlocks);
}

void Records::clear(){
	evts.clear();
	sockcalls.clear();
	jiffies.clear();
	memory_pressures.clear();
	n_memory_allocated = 0;
	n_sockets_allocated = 0;
	memset(mstamp, 0, sizeof(mstamp));
	memset(effect_bool, 0 , sizeof(effect_bool));
}
