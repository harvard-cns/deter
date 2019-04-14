#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <ctime>
#include <pthread.h>
#include <cassert>

#include "deter_recorder.hpp"
#include "mem_share.hpp"
#include "records.hpp"

using namespace std;

#define PAGE_SIZE (4*1024)

vector<Records> res(N_RECORDER);
vector<Records> res2(N_RECORDER);
SharedMemLayout* shmem;

inline uint64_t get_time(){
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * 1000000000lu + ts.tv_nsec;
}

void print_sockcall(derand_rec_sockcall &sc){
	if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
		fprintf(stdout, "sendmsg: 0x%x %lu thread %lu", sc.sendmsg.flags, sc.sendmsg.size, sc.thread_id);
	}else if (sc.type == DERAND_SOCKCALL_TYPE_RECVMSG){
		fprintf(stdout, "recvmsg: 0x%x %lu thread %lu", sc.recvmsg.flags, sc.recvmsg.size, sc.thread_id);
	}else if (sc.type == DERAND_SOCKCALL_TYPE_CLOSE){
		fprintf(stdout, "close: %ld thread %lu", sc.close.timeout, sc.thread_id);
	}else if (sc.type == DERAND_SOCKCALL_TYPE_SPLICE_READ){
		fprintf(stdout, "splice_read: 0x%x %lu thread %lu", sc.splice_read.flags, sc.splice_read.size, sc.thread_id);
	}else if (sc.type == DERAND_SOCKCALL_TYPE_SETSOCKOPT){
		if (valid_rec_setsockopt(&sc.setsockopt)){
			fprintf(stdout, "setsockopt: %hhu %hhu %hhu ", sc.setsockopt.level, sc.setsockopt.optname, sc.setsockopt.optlen);
			for (int j = 0; j < sc.setsockopt.optlen; j++)
				fprintf(stdout, " %hhx", sc.setsockopt.optval[j]);
			fprintf(stdout, " thread %lu", sc.thread_id);
		}else 
			fprintf(stdout, "Error: unsupported setsockopt\n");
	}else{
		fprintf(stdout, "unknown type: %hhu\n", sc.type);
	}
	fprintf(stdout, "\n");
}
bool compare_sockcall(derand_rec_sockcall &sc1, derand_rec_sockcall &sc2){
	if (sc1.type != sc2.type)
		return false;
	if (sc1.type == DETER_SOCKCALL_TYPE_SENDMSG){
		if (sc1.sendmsg.flags != sc2.sendmsg.flags || sc1.sendmsg.size != sc2.sendmsg.size)
			return false;
	}else if (sc1.type == DETER_SOCKCALL_TYPE_RECVMSG){
		if (sc1.recvmsg.flags != sc2.recvmsg.flags || sc1.recvmsg.size != sc2.recvmsg.size)
			return false;
	}else if (sc1.type == DETER_SOCKCALL_TYPE_CLOSE){
		if (sc1.close.timeout != sc2.close.timeout)
			return false;
	}else if (sc1.type == DETER_SOCKCALL_TYPE_SPLICE_READ){
		if (sc1.splice_read.flags != sc2.splice_read.flags || sc1.splice_read.size != sc2.splice_read.size)
			return false;
	}else if (sc1.type == DETER_SOCKCALL_TYPE_SETSOCKOPT){
		if (sc1.setsockopt.level != sc2.setsockopt.level || sc1.setsockopt.optname != sc2.setsockopt.optname || sc1.setsockopt.optlen != sc2.setsockopt.optlen || memcmp(sc1.setsockopt.optval, sc2.setsockopt.optval, sc1.setsockopt.optlen) != 0)
			return false;
	}
	return true;
}

bool compare_records(Records &a, Records &b){
	if (a.evts.size() != b.evts.size()){
		printf("evts size diff: %lu %lu\n", a.evts.size(), b.evts.size());
		return false;
	}
	for (uint32_t i = 0; i < a.evts.size(); i++)
		if (memcmp(&a.evts[i], &b.evts[i], sizeof(a.evts[i])) != 0){
			printf("evts[%u] mismatch: %lu %lu\n", i, *(uint64_t*)&a.evts[i], *(uint64_t*)&b.evts[i]);
			return false;
		}
	if (a.sockcalls.size() != b.sockcalls.size()){
		printf("sockcalls size diff: %lu %lu\n", a.sockcalls.size(), b.sockcalls.size());
		return false;
	}
	for (uint32_t i = 0; i < a.sockcalls.size(); i++)
		if (!compare_sockcall(a.sockcalls[i], b.sockcalls[i])){
			printf("sockcalls[%u] mismatch:\n", i);
			print_sockcall(a.sockcalls[i]);
			print_sockcall(b.sockcalls[i]);
			return false;
		}
	if (a.dpq.size() != b.dpq.size()){
		printf("dpq size diff: %lu %lu\n", a.dpq.size(), b.dpq.size());
		return false;
	}
	for (uint32_t i = 0; i < a.dpq.size(); i++)
		if (memcmp(&a.dpq[i], &b.dpq[i], sizeof(a.dpq[i])) != 0){
			printf("dpq[%u] mismatch: %u %u\n", i, a.dpq[i], b.dpq[i]);
			return false;
		}
	if (a.jiffies.size() != b.jiffies.size()){
		printf("jiffies size diff: %lu %lu\n", a.jiffies.size(), b.jiffies.size());
		return false;
	}
	for (uint32_t i = 0; i < a.jiffies.size(); i++)
		if (memcmp(&a.jiffies[i], &b.jiffies[i], sizeof(a.jiffies[i])) != 0){
			printf("jiffies[%u] mismatch: %lu %lu\n", i, *(uint64_t*)&a.jiffies[i], *(uint64_t*)&b.jiffies[i]);
			return false;
		}
	if (a.mpq.v.size() != b.mpq.v.size()){
		printf("mpq.v size diff: %lu %lu\n", a.mpq.v.size(), b.mpq.v.size());
		return false;
	}
	for (uint32_t i = 0; i < a.mpq.v.size(); i++)
		if (memcmp(&a.mpq.v[i], &b.mpq.v[i], sizeof(a.mpq.v[i])) != 0){
			printf("mpq.v[%u] mismatch: %u %u\n", i, a.mpq.v[i], b.mpq.v[i]);
			return false;
		}
	if (a.memory_allocated.size() != b.memory_allocated.size()){
		printf("memory_allocated size diff: %lu %lu\n", a.memory_allocated.size(), b.memory_allocated.size());
		return false;
	}
	for (uint32_t i = 0; i < a.memory_allocated.size(); i++)
		if (memcmp(&a.memory_allocated[i], &b.memory_allocated[i], sizeof(a.memory_allocated[i])) != 0){
			printf("memory_allocated[%u] mismatch: %lu %lu\n", i, *(uint64_t*)&a.memory_allocated[i], *(uint64_t*)&b.memory_allocated[i]);
			return false;
		}
	if (a.mstamp.size() != b.mstamp.size()){
		printf("mstamp size diff: %lu %lu\n", a.mstamp.size(), b.mstamp.size());
		return false;
	}
	for (uint32_t i = 0; i < a.mstamp.size(); i++)
		if (memcmp(&a.mstamp[i], &b.mstamp[i], sizeof(a.mstamp[i])) != 0){
			printf("mstamp[%u] mismatch: %lu %lu\n", i, *(uint64_t*)&a.mstamp[i], *(uint64_t*)&b.mstamp[i]);
			return false;
		}
	// TODO siqq
	if (a.siq.n != b.siqq.size()){
		printf("siq size diff: %u %lu\n", a.siq.n, b.siqq.size());
		return false;
	}
	for (uint32_t i = 0; i < a.siq.n; i++){
		if (((a.siq.v[i / 32] >> (i & 31)) & 1) != b.siqq[i]){
			printf("siq[%u] mismatch\n", i);
			return false;
		}
	}

	for (uint32_t loc = 0; loc < DETER_EFFECT_BOOL_N_LOC; loc++){
		if (a.ebq[loc].v.size() != b.ebq[loc].v.size()){
			printf("ebq[%u] size diff: %lu %lu\n", loc, a.ebq[loc].v.size(), b.ebq[loc].v.size());
			return false;
		}
		for (uint32_t i = 0; i < a.ebq[loc].v.size(); i++)
			if (memcmp(&a.ebq[loc].v[i], &b.ebq[loc].v[i], sizeof(a.ebq[loc].v[i])) != 0){
				printf("ebq[%u].v[%u] mismatch: %u %u\n", loc, i, a.ebq[loc].v[i], b.ebq[loc].v[i]);
				return false;
			}
	}

	if (a.tsq.size() != b.tsq.size()){
		printf("tsq size diff: %lu %lu\n", a.tsq.size(), b.tsq.size());
		return false;
	}
	for (uint32_t i = 0; i < a.tsq.size(); i++)
		if (memcmp(&a.tsq[i], &b.tsq[i], sizeof(a.tsq[i])) != 0){
			printf("tsq[%u] mismatch: %u %u\n", i, a.tsq[i], b.tsq[i]);
			return false;
		}
	#if ADVANCED_EVENT_ENABLE
	if (a.aeq.size() != b.aeq.size()){
		printf("aeq size diff: %lu %lu\n", a.aeq.size(), b.aeq.size());
		return false;
	}
	for (uint32_t i = 0; i < a.aeq.size(); i++)
		if (memcmp(&a.aeq[i], &b.aeq[i], sizeof(a.aeq[i])) != 0){
			printf("aeq[%u] mismatch: %u %u\n", i, a.aeq[i], b.aeq[i]);
			return false;
		}
	#endif

	printf("compare pass\n");
	return true;
}

void* recorder_func(void *args){
	while (1){
		volatile uint32_t &h = shmem->done_mb_ring.h, &t = shmem->done_mb_ring.t;
		// check done_mb_ring
		if (h == t)
			continue;

		// now we assume only a single thread, which should be the case. But if we need multiple-thread, the following getting mb_idx should be changed
		uint32_t mb_idx = shmem->done_mb_ring.v[get_done_mb_ring_idx(h++)];

		// the mb to dump
		if (mb_idx >= N_MEM_BLOCK) printf("Error: mb_idx=%u > %u\n", mb_idx, N_MEM_BLOCK);
		MemBlock* mb = &shmem->mem_block[mb_idx];

		// the rec of the mb
		if (mb->rec_id >= N_RECORDER) printf("Error: rec_id=%u > %u\n", mb->rec_id, N_RECORDER);
		DeterRecorder* rec = &shmem->recorder[mb->rec_id];

		// the corresponding Records
		Records &r = res[mb->rec_id];

		// if this Records is not active, activate it, and record init data
		if (!r.active){
			r.active = 1;
			r.mode = rec->mode;
			r.sip = ntohl(rec->sip);
			r.dip = ntohl(rec->dip);
			r.sport = ntohs(rec->sport);
			r.dport = ntohs(rec->dport);
			r.init_data = rec->init_data;
		}

		// copy data
		if (mb->type == DETER_MEM_BLOCK_TYPE_EVT){
			derand_event *data = (derand_event*)mb->data;
			r.evts.insert(r.evts.end(), data, data + mb->len);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_SOCKCALL){
			derand_rec_sockcall *data = (derand_rec_sockcall*)mb->data;
			r.sockcalls.insert(r.sockcalls.end(), data, data + mb->len);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_DP){
			uint32_t *data = (uint32_t*)mb->data;
			r.dpq.insert(r.dpq.end(), data, data + mb->len);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_JIF){
			jiffies_rec *data = (jiffies_rec*)mb->data;
			r.jiffies.insert(r.jiffies.end(), data, data + mb->len);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_MP){
			uint32_t len_32b = (mb->len == 0) ? 0 : (mb->len - 1) / 32 + 1; // the number of 32-bit in the mb, round up
			uint32_t *data = (uint32_t*)mb->data;
			r.mpq.v.insert(r.mpq.v.end(), data, data + len_32b);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_MA){
			memory_allocated_rec *data = (memory_allocated_rec*)mb->data;
			r.memory_allocated.insert(r.memory_allocated.end(), data, data + mb->len);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_MS){
			skb_mstamp *data = (skb_mstamp*)mb->data;
			r.mstamp.insert(r.mstamp.end(), data, data + mb->len);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_SIQ){
			uint32_t len_32b = (mb->len == 0) ? 0 : (mb->len - 1) / 32 + 1;
			uint32_t *data = (uint32_t*)mb->data;
			r.siq.v.insert(r.siq.v.end(), data, data + len_32b);
		}else if (mb->type == DETER_MEM_BLOCK_TYPE_TS){
			uint32_t *data = (uint32_t*)mb->data;
			r.tsq.insert(r.tsq.end(), data, data + mb->len);
		}else if (mb->type >= DETER_MEM_BLOCK_TYPE_EB(0) && mb->type < DETER_MEM_BLOCK_TYPE_EB(DETER_EFFECT_BOOL_N_LOC)){
			uint32_t len_32b = (mb->len == 0) ? 0 : (mb->len - 1) / 32 + 1;
			uint32_t *data = (uint32_t*)mb->data;
			int loc = mb->type - DETER_MEM_BLOCK_TYPE_EB(0);
			r.ebq[loc].v.insert(r.ebq[loc].v.end(), data, data + len_32b);
		}
		#if ADVANCED_EVENT_ENABLE
		else if (mb->type == DETER_MEM_BLOCK_TYPE_AE){
			uint32_t *data = (uint32_t*)mb->data;
			r.aeq.insert(r.aeq.end(), data, data + mb->len);
		}
		#endif

		// inc dump_mb
		rec->dump_mb++;

		// if this is the last mb of the rec, this rec is finished
		if (rec->used_mb == rec->dump_mb){
			// finish r
			r.alert = rec->alert;
			r.fin_seq = rec->pkt_idx.fin_seq;
			// set mpq.n
			r.mpq.n = rec->mp.n;
			// copy n_sockets_allocated
			r.n_sockets_allocated = rec->n_sockets_allocated;
			// set eb[k].n
			for (int k = 0; k < DETER_EFFECT_BOOL_N_LOC; k++){
				r.ebq[k].n = rec->eb[k].n;
			}
			// set siq.n
			r.siq.n = rec->siq.n;

			#if USE_DEPRECATED
			/***************************
			 * old version
			 **************************/
			{
				Records &r2 = res2[mb->rec_id];
				// copy
				uint32_t j;
				// copy events
				r2.broken |= (rec->evt_h + DERAND_EVENT_PER_SOCK < rec->evt_t) << 0;
				for (j = rec->evt_h; j < rec->evt_t; j++)
					r2.evts.push_back(rec->evts[get_evt_q_idx(j)]);
				rec->evt_h = j;
				// copy sockcalls
				r2.broken |= (rec->sc_h + DERAND_SOCKCALL_PER_SOCK < rec->sc_t) << 1;
				for (j = rec->sc_h; j < rec->sc_t; j++)
					r2.sockcalls.push_back(rec->sockcalls[get_sc_q_idx(j)]);
				rec->sc_h = j;
				// copy drop
				r2.broken |= (rec->dpq.h + DERAND_DROP_PER_SOCK < rec->dpq.t) << 2;
				for (j = rec->dpq.h; j < rec->dpq.t; j++)
					r2.dpq.push_back(rec->dpq.v[get_drop_q_idx(j)]);
				rec->dpq.h = j;
				#if GET_REORDER
				// copy reorder
				r2.broken |= (rec->reorder.h + DERAND_REORDER_BUF_SIZE < rec->reorder.t) << 3;
				for (j = rec->reorder.h; j < rec->reorder.t; j++)
					r2.reorder.push_back(rec->reorder.buf[get_reorder_buf_idx(j)]);
				rec->reorder.h = j;
				#endif
				// copy jiffies
				r2.broken |= (rec->jf.h + DERAND_JIFFIES_PER_SOCK < rec->jf.t) << 4;
				for (j = rec->jf.h; j < rec->jf.t; j++)
					r2.jiffies.push_back(rec->jf.v[get_jiffies_q_idx(j)]);
				rec->jf.h = j;
				// copy memory pressure
				r2.broken |= (rec->mpq.h + DERAND_MEMORY_PRESSURE_PER_SOCK < rec->mpq.t) << 5;
				for (j = rec->mpq.h; j < (rec->mpq.t & (~31)); j += 32)
					r2.mpq.push_back(rec->mpq.v[get_memory_pressure_q_idx(j) / 32]);
				rec->mpq.h = j;
				// copy memory_allocated
				r2.broken |= (rec->maq.h + DERAND_MEMORY_ALLOCATED_PER_SOCK < rec->maq.t) << 6;
				for (j = rec->maq.h; j < rec->maq.t; j++)
					r2.memory_allocated.push_back(rec->maq.v[get_memory_allocated_q_idx(j)]);
				rec->maq.h = j;
				// copy mstamp
				r2.broken |= (rec->msq.h + DERAND_MSTAMP_PER_SOCK < rec->msq.t) << 7;
				for (j = rec->msq.h; j < rec->msq.t; j++)
					r2.mstamp.push_back(rec->msq.v[get_mstamp_q_idx(j)]);
				rec->msq.h = j;
				// copy skb_still_in_host_queue
				r2.broken |= (rec->siqq.h + DERAND_SKB_IN_QUEUE_PER_SOCK < rec->siqq.t) << 8;
				for (j = rec->siqq.h; j < rec->siqq.t; j++)
					r2.siqq.push_back(rec->siqq.v[get_siq_q_idx(j)]);
				rec->siqq.h = j;
				// copy effect_bool
				// for each effect bool location
				for (int k = 0; k < DERAND_EFFECT_BOOL_N_LOC; k++){
					effect_bool_q &ebq = rec->ebq[k];
					r2.broken |= (ebq.h + DERAND_EFFECT_BOOL_PER_SOCK < ebq.t) << (9 + k);
					for (j = ebq.h; j < (ebq.t & (~31)); j += 32)
						r2.ebq[k].push_back(ebq.v[get_effect_bool_q_idx(j) / 32]);
					ebq.h = j;
				}
				#if ADVANCED_EVENT_ENABLE
				r2.broken |= (rec->aeq.h + DERAND_ADVANCED_EVENT_PER_SOCK < rec->aeq.t) << (9 + DERAND_EFFECT_BOOL_N_LOC);
				for (j = rec->aeq.h; j < rec->aeq.t; j++){
					r2.aeq.push_back(rec->aeq.v[get_aeq_idx(j)]);
				}
				rec->aeq.h = j;
				#endif
				#if COLLECT_TX_STAMP
				r2.broken |= (rec->tsq.h + DERAND_TX_STAMP_PER_SOCK < rec->tsq.t) << (11 + DERAND_EFFECT_BOOL_N_LOC);
				for (j = rec->tsq.h; j < rec->tsq.t; j++){
					r2.tsq.push_back(rec->tsq.v[get_tsq_idx(j)]);
				}
				rec->tsq.h = j;
				#endif
				#if COLLECT_RX_STAMP
				r2.broken |= (rec->rsq.h + DERAND_RX_STAMP_PER_SOCK < rec->rsq.t) << (12 + DERAND_EFFECT_BOOL_N_LOC);
				for (j = rec->rsq.h; j < rec->rsq.t; j++){
					r2.rsq.push_back(rec->rsq.v[get_rsq_idx(j)]);
				}
				rec->rsq.h = j;
				#endif
				#if GET_RX_PKT_IDX
				r2.broken |= (rec->rpq.h + DERAND_RX_PKT_PER_SOCK < rec->rpq.t) << (13 + DERAND_EFFECT_BOOL_N_LOC);
				for (j = rec->rpq.h; j < rec->rpq.t; j++)
					r2.rpq.push_back(rec->rpq.v[get_rx_pkt_q_idx(j)]);
				rec->rpq.h = j;
				#endif
				if (true){
					r2.broken |= rec->broken;
					r2.alert = rec->alert;
					r2.fin_seq = rec->pkt_idx.fin_seq;
					// record the last idx_delta for jiffies
					if (rec->jf.idx_delta > 0){
						jiffies_rec jf_rec;
						jf_rec.jiffies_delta = 0;
						jf_rec.idx_delta = rec->jf.idx_delta;
						r2.jiffies.push_back(jf_rec);
					}

					// copy the last few bits for memory pressure
					if (rec->mpq.t & 31)
						r2.mpq.push_back(rec->mpq.v[get_memory_pressure_q_idx(rec->mpq.t) / 32]);
					r2.mpq.n = rec->mpq.t;

					// record the last idx_delta for memory_allocated
					if (rec->maq.idx_delta > 0){
						memory_allocated_rec ma_rec;
						ma_rec.v_delta = 0;
						ma_rec.idx_delta = rec->maq.idx_delta;
						r2.memory_allocated.push_back(ma_rec);
					}

					r2.n_sockets_allocated = rec->n_sockets_allocated;

					// copy the last few bits for effect_bool
					for (int k = 0; k < DERAND_EFFECT_BOOL_N_LOC; k++){
						effect_bool_q &ebq = rec->ebq[k];
						if (ebq.t & 31)
							r2.ebq[k].push_back(ebq.v[get_effect_bool_q_idx(ebq.t) / 32]);
						r2.ebq[k].n = ebq.t;
					}

					if (r2.alert)
						printf("Alert %x!!! ", r2.alert);
					printf("%08x:%hu-%08x:%hu\t%u %d fin:%u %s #drop %lu\n", r2.sip, r2.sport, r2.dip, r2.dport, rec->evt_t, rec->sockcall_id.counter, rec->pkt_idx.fin_seq, r2.broken? "broken":"ok", r2.dpq.size());
					r2.recorder_id++;
				}

				// compare r and r2
				compare_records(r, r2);
				r2.dump("tmp.rec");
				r2.clear();
			}
			#endif /* USE_DEPRECATED */

			// put rec to free_rec_ring
			shmem->free_rec_ring.v[get_rec_ring_idx(shmem->free_rec_ring.t++)] = mb->rec_id;

			// print
			if (r.alert)
				printf("Alert %x!!! ", r.alert);
			printf("%08x:%hu-%08x:%hu\t%lu %lu fin:%u #drop %lu\n", r.sip, r.sport, r.dip, r.dport, r.evts.size(), r.sockcalls.size(), r.fin_seq, r.dpq.size());

			// dump r
			r.dump();
			r.clear();
			r.active = 0; // deactivate
		}

		// put mb to free_mb_ring
		shmem->free_mb_ring.v[get_free_mb_ring_idx(shmem->free_mb_ring.t++)] = mb_idx;
	}
	return NULL;
}

int main(int argc, char** argv)
{
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem("deter", sizeof(SharedMemLayout)))
		return -1;
	shmem = (SharedMemLayout*)kmem.buf;

	recorder_func(NULL);

	kmem.unmap_mem();

	return 0;
}
