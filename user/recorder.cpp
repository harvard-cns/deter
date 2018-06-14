#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <ctime>

#include "derand.h"
#include "mem_share.hpp"
#include "records.hpp"

using namespace std;

#define PAGE_SIZE (4*1024)
#define N_RECORDER 16

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
	// Currently use a large recorder pool and FIFO recorder recycling to avoid the concurrency bugs
	uint64_t next_t = get_time() / 1000;
	while (1){
		for (int i = 0; i < N_RECORDER; i++){
			derand_recorder* rec = recorders + i;

			// if this recorder has changed to a new socket
			uint32_t d = (rec->recorder_id & (~0x1)) - (res[i].recorder_id & (~0x1));
			if (d > 0){
				if (!(res[i].recorder_id & 1)){ // currently recording
					// dump
					res[i].dump();
					res[i].clear();

					// report
					fprintf(stderr, "jump over %u: %u->%u\n", d, res[i].recorder_id, rec->recorder_id);
				}

				// start new
				res[i].recorder_id = rec->recorder_id & (~0x1); // switch to the new socket's recording state
				res[i].sip = ntohl(rec->sip);
				res[i].dip = ntohl(rec->dip);
				res[i].sport = ntohs(rec->sport);
				res[i].dport = ntohs(rec->dport);
				res[i].init_data = rec->init_data;
			}

			// if currently recording
			if (!(res[i].recorder_id & 1)){
				// copy
				uint32_t j;
				// copy events
				for (j = rec->evt_h; j < rec->evt_t; j++)
					res[i].evts.push_back(rec->evts[get_evt_q_idx(j)]);
				rec->evt_h = j;
				// copy sockcalls
				for (j = rec->sc_h; j < rec->sockcall_id.counter; j++)
					res[i].sockcalls.push_back(rec->sockcalls[get_sc_q_idx(j)]);
				rec->sc_h = j;
				// copy jiffies
				for (j = rec->jf.h; j < rec->jf.t; j++)
					res[i].jiffies.push_back(rec->jf.v[get_jiffies_q_idx(j)]);
				rec->jf.h = j;
				// copy memory pressure
				for (j = rec->mpq.h; j < (rec->mpq.t & (~31)); j += 32)
					res[i].memory_pressures.push_back(rec->mpq.v[get_memory_pressure_q_idx(j) / 32]);
				rec->mpq.h = j;


				// if the socket's recorder has finished
				if (rec->recorder_id - res[i].recorder_id == 1){
					// copy the last few bits for memory pressure
					if (rec->mpq.t & 31)
						res[i].memory_pressures.push_back(rec->mpq.v[get_memory_pressure_q_idx(rec->mpq.t) / 32]);
					// copy n_memory_allocated
					res[i].n_memory_allocated = rec->n_memory_allocated;
					res[i].n_sockets_allocated = rec->n_sockets_allocated;
					memcpy(res[i].mstamp, rec->mstamp, sizeof(rec->mstamp));
					memcpy(res[i].effect_bool, rec->effect_bool, sizeof(rec->effect_bool));

					printf("%u %d\n", rec->evt_t, rec->sockcall_id.counter);
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
