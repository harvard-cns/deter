#include <cstring>
#include <unordered_map>
#include <cmath>
#include <map>
#include "records.hpp"

using namespace std;

template <typename T>
int dump_vector(const vector<T> &v, FILE *fout){
	uint32_t len = v.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		return 0;
	if (len > 0)
		if (!fwrite(&v[0], sizeof(T) * len, 1, fout))
			return 0;
	return 1;
}

template <typename T>
int read_vector(vector<T> &v, FILE *fin){
	uint32_t len;
	if (!fread(&len, sizeof(len), 1, fin))
		return 0;
	v.resize(len);
	if (len > 0)
		if (!fread(&v[0], sizeof(T) * len, 1, fin))
			return 0;
	return 1;
}

void Records::transform(){
	// transform sockcalls
	unordered_map<u64, u64> ids;
	for (uint32_t i = 0; i < sockcalls.size(); i++){
		u64 key = sockcalls[i].thread_id;
		if (ids.find(key) == ids.end()){ // a new thread
			sockcalls[i].thread_id = ids.size();
			ids[key] = sockcalls[i].thread_id;
		}else { // a existing thread
			sockcalls[i].thread_id = ids[key];
		}
	}
	// transform mpq
	mpq.transform_to_idx_one();
}

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

	// transform raw data into final format 
	transform();

	// write mode
	if (!fwrite(&mode, sizeof(mode), 1, fout))
		goto fail_write;

	// write broken 
	if (!fwrite(&broken, sizeof(broken), 1, fout))
		goto fail_write;

	// write 4 tuples
	if (!fwrite(&sip, sizeof(sip)+sizeof(dip)+sizeof(sport)+sizeof(dport), 1, fout))
		goto fail_write;

	// write init_data
	if (!fwrite(&init_data, sizeof(init_data), 1, fout))
		goto fail_write;

	// write events
	if (!dump_vector(evts, fout))
		goto fail_write;

	// write sockcalls
	if (!dump_vector(sockcalls, fout))
		goto fail_write;

	// write drop
	if (!dump_vector(dpq, fout))
		goto fail_write;

	// write jiffies_reads
	if (!dump_vector(jiffies, fout))
		goto fail_write;

	// write memory_pressures
	if (!mpq.dump(fout))
		goto fail_write;

	// write memory_allocated
	if (!dump_vector(memory_allocated, fout))
		goto fail_write;

	// write n_sockets_allocated
	if (!fwrite(&n_sockets_allocated, sizeof(n_sockets_allocated), 1, fout))
		goto fail_write;

	// write mstamp
	if (!dump_vector(mstamp, fout))
		goto fail_write;

	// write siqq
	if (!dump_vector(siqq, fout))
		goto fail_write;

	// write effect_bool
	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		if (!ebq[i].dump(fout))
			goto fail_write;

	#if DERAND_DEBUG
	// write geq
	if (!dump_vector(geq, fout))
		goto fail_write;
	#endif

	fclose(fout);
	return 0;
fail_write:
	fclose(fout);
	return -1;
}

int Records::read(const char* filename){
	FILE* fin= fopen(filename, "r");
	uint32_t len;
	// read mode
	if (!fread(&mode, sizeof(mode), 1, fin))
		goto fail_read;

	// read broken
	if (!fread(&broken, sizeof(broken), 1, fin))
		goto fail_read;

	// read 4 tuples
	if (!fread(&sip, sizeof(sip)+sizeof(dip)+sizeof(sport)+sizeof(dport), 1, fin))
		goto fail_read;

	// read init_data
	if (!fread(&init_data, sizeof(init_data), 1, fin))
		goto fail_read;

	// read events
	if (!read_vector(evts, fin))
		goto fail_read;

	// read sockcalls
	if (!read_vector(sockcalls, fin))
		goto fail_read;

	// read drop
	if (!read_vector(dpq, fin))
		goto fail_read;

	// read jiffies_reads
	if (!read_vector(jiffies, fin))
		goto fail_read;

	// read memory_pressures
	if (!mpq.read(fin))
		goto fail_read;

	// read memory_allocated
	if (!read_vector(memory_allocated, fin))
		goto fail_read;
	
	// read n_sockets_allocated
	if (!fread(&n_sockets_allocated, sizeof(n_sockets_allocated), 1, fin))
		goto fail_read;

	// read mstamp
	if (!read_vector(mstamp, fin))
		goto fail_read;

	// read siqq
	if (!read_vector(siqq, fin))
		goto fail_read;

	// read effect_bool
	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		if (!ebq[i].read(fin))
			goto fail_read;

	#if DERAND_DEBUG
	// read geq
	if (!read_vector(geq, fin))
		goto fail_read;
	#endif

	fclose(fin);
	return 0;
fail_read:
	fclose(fin);
	return -1;
}

void Records::print(FILE* fout){
	fprintf(fout, "broken: %x\n", broken);
	fprintf(fout, "mode: %u\n", mode);
	fprintf(fout, "%08x:%hu %08x:%hu\n", sip, sport, dip, dport);
	fprintf(fout, "%lu sockcalls\n", sockcalls.size());
	for (int i = 0; i < sockcalls.size(); i++){
		derand_rec_sockcall &sc = sockcalls[i];
		if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
			fprintf(fout, "sendmsg: 0x%x %lu thread %lu\n", sc.sendmsg.flags, sc.sendmsg.size, sc.thread_id);
		}else if (sc.type == DERAND_SOCKCALL_TYPE_RECVMSG){
			fprintf(fout, "recvmsg: 0x%x %lu thread %lu\n", sc.recvmsg.flags, sc.recvmsg.size, sc.thread_id);
		}else if (sc.type == DERAND_SOCKCALL_TYPE_CLOSE){
			fprintf(fout, "close: %ld thread %lu\n", sc.close.timeout, sc.thread_id);
		}else if (sc.type == DERAND_SOCKCALL_TYPE_SPLICE_READ){
			fprintf(fout, "splice_read: 0x%x %lu thread %lu\n", sc.splice_read.flags, sc.splice_read.size, sc.thread_id);
		}
	}
	fprintf(fout, "%lu events\n", evts.size());
	for (int i = 0; i < evts.size(); i++){
		derand_event &e = evts[i];
		char buf[32];
		fprintf(fout, "%u %s", e.seq, get_event_name(e.type, buf));
		if (e.type >= DERAND_SOCK_ID_BASE)
			fprintf(fout, " %s", get_sockcall_str(&sockcalls[get_sockcall_idx(e.type)], buf));

		#if DERAND_DEBUG
		fprintf(fout, " %u", e.dbg_data);
		#endif
		fprintf(fout, "\n");
	}

	fprintf(fout, "%lu drops\n", dpq.size());
	for (uint32_t i = 0; i < dpq.size(); i++)
		fprintf(fout, "%u\n", dpq[i]);

	fprintf(fout, "%lu new jiffies\n", jiffies.size());
	if (jiffies.size() > 0){
		fprintf(fout, "first: %lu\n", jiffies[0].init_jiffies);
		for (int i = 1; i < jiffies.size(); i++)
			fprintf(fout, "%u %u\n", jiffies[i].idx_delta, jiffies[i].jiffies_delta);
	}

	fprintf(fout, "memory_pressure:\n");
	mpq.print(fout);

	fprintf(fout, "%lu new values of reading memory_allocated\n", memory_allocated.size());
	if (memory_allocated.size() > 0){
		fprintf(fout, "first: %lu\n", memory_allocated[0].init_v);
		for (int i = 1; i < memory_allocated.size(); i++)
			fprintf(fout, "%u %d\n", memory_allocated[i].idx_delta, memory_allocated[i].v_delta);
	}

	fprintf(fout, "%u reads to n_sockets_allocated\n", n_sockets_allocated);

	fprintf(fout, "%lu skb_mstamp_get:\n", mstamp.size());
	for (int64_t i = 0; i < mstamp.size(); i++)
		fprintf(fout, "%u %u\n", mstamp[i].stamp_us, mstamp[i].stamp_jiffies);

	fprintf(fout, "%lu skb_still_in_host_queue:\n", siqq.size());
	for (int64_t i = 0; i < siqq.size(); i++)
		fprintf(fout, "%hhu\n", siqq[i]);

	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++){
		auto &eb = ebq[i];
		fprintf(fout, "effect_bool %d: %u reads %lu\n", i, eb.n, eb.v.size());
		for (int j = 0; j < eb.v.size(); j++){
			for (uint32_t k = 0, b = eb.v[j]; k < 32; k++, b>>=1)
				fprintf(fout, "%u", b & 1);
			fprintf(fout, "\n");
		}
	}

	#if DERAND_DEBUG
	fprintf(fout, "%lu general events\n", geq.size());
	for (int i = 0; i < geq.size(); i++){
		u32 type = geq[i].type;
		u64 data = *(u64*)geq[i].data;
		char buf[32];
		fprintf(fout, "%d ", i);
		// 0: evtq; 1: jfq; 2: mpq; 3: maq; 4: saq; 5: msq; 6 ~ 6+DERAND_EFFECT_BOOL_N_LOC-1: ebq
		fprintf(fout, "%s", get_ge_name(type, buf));
		if (type == 0)
			fprintf(fout, " %s", get_event_name(data, buf));
		else
			fprintf(fout, " %lu", data);
		fprintf(fout, "\n");
	}
	#endif
}

void Records::print_init_data(FILE* fout){
	tcp_sock_init_data *d = &init_data;
	fprintf(fout, "tcp_header_len = %u\n", d->tcp_header_len);
	fprintf(fout, "pred_flags = %u\n", d->pred_flags);
	fprintf(fout, "segs_in = %u\n", d->segs_in);
	fprintf(fout, "rcv_nxt = %u\n", d->rcv_nxt);
	fprintf(fout, "copied_seq = %u\n", d->copied_seq);
	fprintf(fout, "rcv_wup = %u\n", d->rcv_wup);
	fprintf(fout, "snd_nxt = %u\n", d->snd_nxt);
	fprintf(fout, "segs_out = %u\n", d->segs_out);
	fprintf(fout, "bytes_acked = %lu\n", d->bytes_acked);
	fprintf(fout, "snd_una = %u\n", d->snd_una);
	fprintf(fout, "snd_sml = %u\n", d->snd_sml);
	fprintf(fout, "rcv_tstamp = %u\n", d->rcv_tstamp);
	fprintf(fout, "lsndtime = %u\n", d->lsndtime);
	fprintf(fout, "snd_wl1 = %u\n", d->snd_wl1);
	fprintf(fout, "snd_wnd = %u\n", d->snd_wnd);
	fprintf(fout, "max_window = %u\n", d->max_window);
	fprintf(fout, "window_clamp = %u\n", d->window_clamp);
	fprintf(fout, "rcv_ssthresh = %u\n", d->rcv_ssthresh);
	fprintf(fout, "rack.mstamp={%u, %u}\n", d->rack.mstamp.stamp_us, d->rack.mstamp.stamp_jiffies);
	fprintf(fout, "rack.advanced=%hhu\n", d->rack.advanced); 
	fprintf(fout, "rack.reord=%hhu\n", d->rack.reord);    
	fprintf(fout, "advmss = %u\n", d->advmss);
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
	fprintf(fout, "rx_opt.rcv_tsval = %u\n", d->rx_opt.rcv_tsval);
	fprintf(fout, "rx_opt.rcv_tsecr = %u\n", d->rx_opt.rcv_tsecr);
	fprintf(fout, "rx_opt.saw_tstamp = %u\n", d->rx_opt.saw_tstamp);
	fprintf(fout, "rx_opt.tstamp_ok = %u\n", d->rx_opt.tstamp_ok);
	fprintf(fout, "rx_opt.dsack = %u\n", d->rx_opt.dsack);
	fprintf(fout, "rx_opt.wscale_ok = %u\n", d->rx_opt.wscale_ok);
	fprintf(fout, "rx_opt.sack_ok = %u\n", d->rx_opt.sack_ok);
	fprintf(fout, "rx_opt.snd_wscale = %u\n", d->rx_opt.snd_wscale);
	fprintf(fout, "rx_opt.rcv_wscale = %u\n", d->rx_opt.rcv_wscale);
	fprintf(fout, "rx_opt.num_sacks = %hhu\n", d->rx_opt.num_sacks);
	fprintf(fout, "rx_opt.user_mss = %hhu\n", d->rx_opt.user_mss);
	fprintf(fout, "rx_opt.mss_clamp = %u\n", d->rx_opt.mss_clamp);
	fprintf(fout, "snd_cwnd = %u\n", d->snd_cwnd);
	fprintf(fout, "snd_cwnd_stamp = %u\n", d->snd_cwnd_stamp);
	fprintf(fout, "rcv_wnd = %u\n", d->rcv_wnd);
	fprintf(fout, "write_seq = %u\n", d->write_seq);
	fprintf(fout, "pushed_seq = %u\n", d->pushed_seq);
	fprintf(fout, "undo_retrans = %d\n", d->undo_retrans);
	fprintf(fout, "total_retrans = %u\n", d->total_retrans);
	fprintf(fout, "rcv_rtt_est = {%u, %u, %u}\n", d->rcv_rtt_est.rtt, d->rcv_rtt_est.seq, d->rcv_rtt_est.time);
	fprintf(fout, "rcvq_space = {%d, %u, %u}\n", d->rcvq_space.space, d->rcvq_space.seq, d->rcvq_space.time);
	fprintf(fout, "icsk_timeout = %lu\n", d->icsk_timeout);
	fprintf(fout, "icsk_rto = %u\n", d->icsk_rto);
	fprintf(fout, "icsk_ack.lrcvtime = %u\n", d->icsk_ack.lrcvtime);
	fprintf(fout, "icsk_ack.last_seg_size = %u\n", d->icsk_ack.last_seg_size);
	fprintf(fout, "icsk_ack.pending = %hhu\n", d->icsk_ack.pending);
	fprintf(fout, "icsk_ack.quick = %hhu\n", d->icsk_ack.quick);
	fprintf(fout, "icsk_ack.pingpong = %hhu\n", d->icsk_ack.pingpong);
	fprintf(fout, "icsk_ack.blocked = %hhu\n", d->icsk_ack.blocked);
	fprintf(fout, "icsk_ack.ato = %u\n", d->icsk_ack.ato);
	fprintf(fout, "icsk_ack.timeout = %lu\n", d->icsk_ack.timeout);
	fprintf(fout, "icsk_ack.lrcvtime = %u\n", d->icsk_ack.lrcvtime);
	fprintf(fout, "icsk_ack.last_seg_size = %hu\n", d->icsk_ack.last_seg_size);
	fprintf(fout, "icsk_ack.rcv_mss = %hu\n", d->icsk_ack.rcv_mss);
	fprintf(fout, "icsk_mtup.enabled = %d\n", d->icsk_mtup.enabled);
	fprintf(fout, "icsk_mtup.search_high = %d\n", d->icsk_mtup.search_high);
	fprintf(fout, "icsk_mtup.search_low = %d\n", d->icsk_mtup.search_low);
	fprintf(fout, "icsk_mtup.probe_size = %d\n", d->icsk_mtup.probe_size);
	fprintf(fout, "icsk_mtup.probe_timestamp = %u\n", d->icsk_mtup.probe_timestamp);
	fprintf(fout, "skc_reuse = %hhu\n", d->skc_reuse);
	fprintf(fout, "skc_reuseport = %hhu\n", d->skc_reuseport);
	fprintf(fout, "skc_flags = %lu\n", d->skc_flags);
	fprintf(fout, "sk_forward_alloc = %d\n", d->sk_forward_alloc);
	fprintf(fout, "sk_ll_usec = %u\n", d->sk_ll_usec);
	fprintf(fout, "sk_rcvbuf = %d\n", d->sk_rcvbuf);
	fprintf(fout, "sk_sndbuf = %d\n", d->sk_sndbuf);
	fprintf(fout, "sk_no_check_tx = %u\n", d->sk_no_check_tx);
	fprintf(fout, "sk_userlocks = %u\n", d->sk_userlocks);
	fprintf(fout, "sk_pacing_rate = %u\n", d->sk_pacing_rate);
	fprintf(fout, "sk_max_pacing_rate = %u\n", d->sk_max_pacing_rate);
	fprintf(fout, "sk_route_caps = %lu\n", d->sk_route_caps);
	fprintf(fout, "sk_route_nocaps = %lu\n", d->sk_route_nocaps);
	fprintf(fout, "sk_rcvlowat = %d\n", d->sk_rcvlowat);
	fprintf(fout, "sk_lingertime = %lu\n", d->sk_lingertime);
	fprintf(fout, "sk_priority = %u\n", d->sk_priority);
	fprintf(fout, "sk_rcvtimeo = %ld\n", d->sk_rcvtimeo);
	fprintf(fout, "sk_sndtimeo = %ld\n", d->sk_sndtimeo);
	fprintf(fout, "sk_tsflags = %hu\n", d->sk_tsflags);
	fprintf(fout, "sk_tskey = %u\n", d->sk_tskey);
	fprintf(fout, "sk_mark = %u\n", d->sk_mark);
}

void Records::clear(){
	mode = 0;
	broken = 0;
	evts.clear();
	sockcalls.clear();
	jiffies.clear();
	mpq.clear();
	memory_allocated.clear();
	n_sockets_allocated = 0;
	mstamp.clear();
	siqq.clear();
	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		ebq[i].clear();
	#if DERAND_DEBUG
	geq.clear();
	#endif
}

uint64_t get_sockcall_key(derand_rec_sockcall &sc){
	uint64_t key = sc.type;
	if (sc.type == DERAND_SOCKCALL_TYPE_CLOSE){
		key <<= 32;
		key |= sc.close.timeout;
		key <<= 30;
	}else if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
		key <<= 32;
		key |= sc.sendmsg.flags;
		key <<= 30;
		key |= sc.sendmsg.size & ((1<<30)-1);
	}else if (sc.type == DERAND_SOCKCALL_TYPE_RECVMSG){
		key <<= 32;
		key |= sc.recvmsg.flags;
		key <<= 30;
		key |= sc.recvmsg.size & ((1<<30)-1);
	}else if (sc.type == DERAND_SOCKCALL_TYPE_SPLICE_READ){
		key <<= 32;
		key |= sc.splice_read.flags;
		key <<= 30;
		key |= sc.splice_read.size & ((1<<30)-1);
	}
	return key;
}

void Records::print_raw_storage_size(){
	uint64_t size = 0;
	size += sizeof(init_data);
	printf("init_data: %lu\n", sizeof(init_data));
	size += sizeof(derand_event) * evts.size();
	printf("evts: %lu\n", sizeof(derand_event) * evts.size());
	size += sizeof(derand_rec_sockcall) * sockcalls.size();
	printf("sockcalls: %lu\n", sizeof(derand_rec_sockcall) * sockcalls.size());
	//size += sizeof(uint32_t) * dpq.size();
	//printf("dpq: %lu\n", sizeof(uint32_t) * dpq.size());
	size += sizeof(jiffies_rec) * jiffies.size();
	printf("jiffies: %lu\n", sizeof(jiffies_rec) * jiffies.size());
	size += mpq.raw_storage_size();
	printf("mpq: %lu\n", mpq.raw_storage_size());
	size += sizeof(memory_allocated_rec) * memory_allocated.size();
	printf("memory_allocated: %lu\n", sizeof(memory_allocated_rec) * memory_allocated.size());
	size += sizeof(skb_mstamp) * mstamp.size();
	printf("mstamp: %lu\n", sizeof(skb_mstamp) * mstamp.size());
	size += sizeof(uint8_t) * siqq.size();
	printf("siqq: %lu\n", sizeof(uint8_t) * siqq.size());
	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++){
		size += ebq[i].raw_storage_size();
		printf("ebq[%d]: %lu\n", i, ebq[i].raw_storage_size());
	}
	printf("total: %lu\n", size);
}

void Records::print_compressed_storage_size(){
	uint64_t size = 0;
	size += sizeof(init_data);
	printf("init_data: %lu\n", sizeof(init_data));
	{
		uint64_t this_size = 0, cnt = 0;
		// | 16bit seq# delta | 2bit code | data (variable size) |
		// if code == 00: data = event type (tasklet/timeout) (6bit)
		// if code == 10: data = # consecutive sockcall (6bit)
		// if code == 11: data = # consecutive sockcall (6bit) | thread_id (8bit)
		for (int i = 0; i < evts.size(); i++){
			if (evts[i].type < DERAND_SOCK_ID_BASE){
				this_size += 3;
			}else {
				uint64_t this_thread_id = sockcalls[get_sockcall_idx(evts[i].type)].thread_id;
				if (i > 0 &&
					evts[i-1].type >= DERAND_SOCK_ID_BASE &&
					sockcalls[get_sockcall_idx(evts[i-1].type)].thread_id == this_thread_id &&
					cnt < 64){
					cnt++;
				}else{
					this_size += 3 + (this_thread_id != 0);
					cnt = 0;
				}
			}
		}
		size += this_size;
		printf("evts: %lu\n", this_size);
	}

	{
		uint64_t this_size = 0;
		// assign a unique id to each unique flag
		map<int,int> flag_id;
		for (int i = 0; i < sockcalls.size(); i++){
			if (sockcalls[i].type != DERAND_SOCKCALL_TYPE_CLOSE && flag_id.find(sockcalls[i].sendmsg.flags) == flag_id.end()){
				uint64_t id = flag_id.size();
				flag_id[sockcalls[i].sendmsg.flags] = id;
			}
		}
		this_size += 4 * flag_id.size();
		// store each sockcall. Normally # diff flag combination <= 16, so flag_id is 4bit
		int cnt = 0, last_thread_id = 0, thread_id_bytes = 0;
		for (int i = 0; i < sockcalls.size(); i++){
			if (i > 0 && get_sockcall_key(sockcalls[i]) == get_sockcall_key(sockcalls[i-1]) && cnt < 16){
				cnt++;
			}else{
				this_size += 4 + thread_id_bytes; // 2bit type, 4bit #consecutive same sockcall, 4bit flag_id, 22bit size, variable thread_id_bytes
				cnt = 0;
			}
			// update storage for thread_id
			if (sockcalls[i].thread_id > last_thread_id){
				if (sockcalls[i].thread_id >= (1 << (thread_id_bytes*8))){
					thread_id_bytes++;
					this_size += 4; // record the idx where thread_id_bytes increases
				}
				last_thread_id = sockcalls[i].thread_id;
			}
		}
		size += this_size;
		printf("sockcalls: %lu\n", this_size);
	}
	//size += sizeof(uint32_t) * dpq.size();
	//printf("dpq: %lu\n", sizeof(uint32_t) * dpq.size());
	size += sizeof(jiffies_rec) * jiffies.size();
	printf("jiffies: %lu\n", sizeof(jiffies_rec) * jiffies.size());
	size += mpq.raw_storage_size();
	printf("mpq: %lu\n", mpq.raw_storage_size());
	size += sizeof(memory_allocated_rec) * memory_allocated.size();
	printf("memory_allocated: %lu\n", sizeof(memory_allocated_rec) * memory_allocated.size());
	{
		uint64_t jiffies_size = 0, us_size = 0;
		// stamp_jiffies
		for (int i = 0; i < mstamp.size(); i++){
			if (i == 0 || mstamp[i].stamp_jiffies != mstamp[i-1].stamp_jiffies)
				jiffies_size += 8; // 32bit jiffies delta, 32bit idx delta
		}
		// stamp_us
		uint64_t us_bits = 0;
		for (int i = 0; i < mstamp.size(); i++){
			if (i == 0 || mstamp[i].stamp_us - mstamp[i-1].stamp_us >= 15)
				us_bits += 4 + 32; // 4bit 0xf (meaning delta >= 15), 32bit delta
			else
				us_bits += 4; // 4bit delta
		}
		us_size = us_bits/8;
		size += jiffies_size + us_size;
		printf("mstamp: jiffies: %lu us: %lu total: %lu\n", jiffies_size, us_size, jiffies_size+us_size);
	}
	size += sizeof(uint8_t) * siqq.size();
	printf("siqq: %lu\n", sizeof(uint8_t) * siqq.size());
	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++){
		size += ebq[i].raw_storage_size();
		printf("ebq[%d]: %lu\n", i, ebq[i].raw_storage_size());
	}
	printf("total: %lu\n", size);
}
