#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <ctime>

#include "derand_recorder.hpp"
#include "mem_share.hpp"
#include "records.hpp"

using namespace std;

#define PAGE_SIZE (4*1024)
#define N_RECORDER 32

inline uint64_t get_time(){
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * 1000000000lu + ts.tv_nsec;
}

int main()
{
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem("derand", N_RECORDER * sizeof(derand_recorder)))
		return -1;
	derand_recorder* recorders = (derand_recorder*)kmem.buf;
	vector<Records> res(N_RECORDER);

	// TODO: there are many potential concurrency bugs here
	// check "Pending" in OneNote
	uint64_t next_t = get_time() / 1000;
	while (1){
		for (int i = 0; i < N_RECORDER; i++){
			derand_recorder* rec = recorders + i;

			// if this recorder has changed to a new socket, we dump current copied data, and switch to new one
			uint32_t d = (rec->recorder_id & (~0x1)) - (res[i].recorder_id & (~0x1));
			if (d > 0){
				// if the copier is still in copy state, this means we may lost some data
				if (!(res[i].recorder_id & 1)){
					// first dump copied data
					res[i].dump();
					res[i].clear();

					// report that we may lost some data
					fprintf(stderr, "jump over %u: %u->%u\n", d, res[i].recorder_id, rec->recorder_id);
				}

				// start new
				res[i].mode = rec->mode;
				res[i].recorder_id = rec->recorder_id & (~0x1); // switch to the new socket's copy state
				res[i].sip = ntohl(rec->sip);
				res[i].dip = ntohl(rec->dip);
				res[i].sport = ntohs(rec->sport);
				res[i].dport = ntohs(rec->dport);
				res[i].init_data = rec->init_data;
			}
			// NOTE: After this, we assume the copier res[i] is on the latest socket's recorder
			//       This may NOT be true, but hopefull copy is fast enough so that the recorder state won't change that much

			// if in copy state
			if (!(res[i].recorder_id & 1)){
				// test whether the current socket's recorder has finished
				bool finished = (rec->recorder_id - res[i].recorder_id == 1); // Potential bug: rec->recorder_id - res[i].recorder_id > 1
				// copy
				uint32_t j;
				// copy events
				res[i].broken |= (rec->evt_h + DERAND_EVENT_PER_SOCK < rec->evt_t) << 0;
				for (j = rec->evt_h; j < rec->evt_t; j++)
					res[i].evts.push_back(rec->evts[get_evt_q_idx(j)]);
				rec->evt_h = j;
				// copy sockcalls
				res[i].broken |= (rec->sc_h + DERAND_SOCKCALL_PER_SOCK < rec->sc_t) << 1;
				for (j = rec->sc_h; j < rec->sockcall_id.counter; j++)
					res[i].sockcalls.push_back(rec->sockcalls[get_sc_q_idx(j)]);
				rec->sc_h = j;
				// copy drop
				res[i].broken |= (rec->dpq.h + DERAND_DROP_PER_SOCK < rec->dpq.t) << 2;
				for (j = rec->dpq.h; j < rec->dpq.t; j++)
					res[i].dpq.push_back(rec->dpq.v[get_drop_q_idx(j)]);
				rec->dpq.h = j;
				// copy jiffies
				res[i].broken |= (rec->jf.h + DERAND_JIFFIES_PER_SOCK < rec->jf.t) << 3;
				for (j = rec->jf.h; j < rec->jf.t; j++)
					res[i].jiffies.push_back(rec->jf.v[get_jiffies_q_idx(j)]);
				rec->jf.h = j;
				// copy memory pressure
				res[i].broken |= (rec->mpq.h + DERAND_MEMORY_PRESSURE_PER_SOCK < rec->mpq.t) << 4;
				for (j = rec->mpq.h; j < (rec->mpq.t & (~31)); j += 32)
					res[i].mpq.push_back(rec->mpq.v[get_memory_pressure_q_idx(j) / 32]);
				rec->mpq.h = j;
				// copy memory_allocated
				res[i].broken |= (rec->maq.h + DERAND_MEMORY_ALLOCATED_PER_SOCK < rec->maq.t) << 5;
				for (j = rec->maq.h; j < rec->maq.t; j++)
					res[i].memory_allocated.push_back(rec->maq.v[get_memory_allocated_q_idx(j)]);
				rec->maq.h = j;
				// copy mstamp
				res[i].broken |= (rec->msq.h + DERAND_MSTAMP_PER_SOCK < rec->msq.t) << 6;
				for (j = rec->msq.h; j < rec->msq.t; j++)
					res[i].mstamp.push_back(rec->msq.v[get_mstamp_q_idx(j)]);
				rec->msq.h = j;
				// copy effect_bool
				// for each effect bool location
				for (int k = 0; k < DERAND_EFFECT_BOOL_N_LOC; k++){
					effect_bool_q &ebq = rec->ebq[k];
					res[i].broken |= (ebq.h + DERAND_EFFECT_BOOL_PER_SOCK < ebq.t) << (7 + k);
					for (j = ebq.h; j < (ebq.t & (~31)); j += 32)
						res[i].ebq[k].push_back(ebq.v[get_effect_bool_q_idx(j) / 32]);
					ebq.h = j;
				}
				#if DERAND_DEBUG
				// copy GeneralEvent
				res[i].broken |= (rec->geq.h + DERAND_GENERAL_EVENT_PER_SOCK < rec->geq.t) << (7 + DERAND_EFFECT_BOOL_N_LOC);
				for (j = rec->geq.h; j < rec->geq.t; j++)
					res[i].geq.push_back(rec->geq.v[get_geq_idx(j)]);
				rec->geq.h = j;
				#endif

				// if the socket's recorder has finished, do the finish job
				// NOTE: we must use the test result before the copying. Otherwise if we test here, there may be new data after copy, which will be lost
				if (finished){
					// record the last idx_delta for jiffies
					if (rec->jf.idx_delta > 0){
						jiffies_rec jf_rec;
						jf_rec.jiffies_delta = 0;
						jf_rec.idx_delta = rec->jf.idx_delta;
						res[i].jiffies.push_back(jf_rec);
					}

					// copy the last few bits for memory pressure
					if (rec->mpq.t & 31)
						res[i].mpq.push_back(rec->mpq.v[get_memory_pressure_q_idx(rec->mpq.t) / 32]);
					res[i].mpq.n = rec->mpq.t;

					// record the last idx_delta for memory_allocated
					if (rec->maq.idx_delta > 0){
						memory_allocated_rec ma_rec;
						ma_rec.v_delta = 0;
						ma_rec.idx_delta = rec->maq.idx_delta;
						res[i].memory_allocated.push_back(ma_rec);
					}

					res[i].n_sockets_allocated = rec->n_sockets_allocated;

					// copy the last few bits for effect_bool
					for (int k = 0; k < DERAND_EFFECT_BOOL_N_LOC; k++){
						effect_bool_q &ebq = rec->ebq[k];
						if (ebq.t & 31)
							res[i].ebq[k].push_back(ebq.v[get_effect_bool_q_idx(ebq.t) / 32]);
						res[i].ebq[k].n = ebq.t;
					}

					printf("%u %d %s", rec->evt_t, rec->sockcall_id.counter, res[i].broken? "broken":"ok");
					res[i].dump();
					res[i].clear();
					res[i].recorder_id++;
				}
			}
		}

		#if 1
		// check if can sleep
		next_t += 1000;
		uint64_t now = get_time() / 1000;
		if (next_t > now)
			usleep(next_t - now);
		#endif
	}

	kmem.unmap_mem();

	return 0;
}
