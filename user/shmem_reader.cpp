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
SharedMemLayout* shmem;

int main(int argc, char** argv)
{
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem("deter", sizeof(SharedMemLayout)))
		return -1;
	shmem = (SharedMemLayout*)kmem.buf;

	/* do things on the shmem */
	printf("free_rec_ring %u %u\n", shmem->free_rec_ring.h, shmem->free_rec_ring.t);
	printf("free_mb_ring %u %u\n", shmem->free_mb_ring.h, shmem->free_mb_ring.t);
	map<uint32_t, int> cnt;
	for (uint32_t i = shmem->free_mb_ring.h; i < shmem->free_mb_ring.t; i++){
		//printf("%u %u\n", i, shmem->free_mb_ring.v[get_free_mb_ring_idx(i)]);
		cnt[shmem->free_mb_ring.v[get_free_mb_ring_idx(i)]]++;
	}
	printf("# diff mb idx in free_mb_ring: %lu\n", cnt.size());
	printf("\n");

	printf("done_mb_ring %u %u\n", shmem->done_mb_ring.h, shmem->done_mb_ring.t);
	#if 0
	for (uint32_t i = shmem->done_mb_ring.h; i < shmem->done_mb_ring.t; i++){
		printf("%u %u\n", i, shmem->done_mb_ring.v[get_done_mb_ring_idx(i)]);
	}
	#endif

	for (uint32_t i = 0; i < N_RECORDER; i++)
		printf("rec[%u]: used_mb: %u dump_mb: %u\n", i, shmem->recorder[i].used_mb, shmem->recorder[i].dump_mb);
	for (uint32_t i = 0; i < 32; i++)
		printf("mb[%u]: offset: %lu rec_id = %u len = %hu type = %hhu\n", i, (uint64_t)&shmem->mem_block[i] - (uint64_t)shmem, shmem->mem_block[i].rec_id, shmem->mem_block[i].len, shmem->mem_block[i].type);

	kmem.unmap_mem();

	return 0;
}
