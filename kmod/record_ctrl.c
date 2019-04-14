#include <linux/slab.h>
#include <linux/mm.h>
#include "record_ctrl.h"
#include "mem_util.h"
#include "record_shmem.h"

int create_record_ctrl(void){
	int i;

	// find the right number of pages 
	int order = get_page_order(sizeof(struct SharedMemLayout));
	printk("[DERAND] need %lu Bytes, allocat %lu Bytes\n", (sizeof(struct SharedMemLayout)), (1<<order) * 4096lu);

	// allocate and reserve pages
	shmem.addr = (struct SharedMemLayout*)__get_free_pages(GFP_KERNEL, order);
	if (shmem.addr){
		reserve_pages(virt_to_page(shmem.addr), 1<<order);
	}else 
		goto fail_addr;

	// init free_rec_ring, contain all recorder
	struct RecorderRing *rec_ring = &shmem.addr->free_rec_ring;
	rec_ring->h = 0;
	rec_ring->t = N_RECORDER;
	for (i = 0; i < N_RECORDER; i++)
		rec_ring->v[i] = i;

	// init free_mb_ring, contain all MemBlock
	struct FreeMemBlockRing *free_mb_ring = &shmem.addr->free_mb_ring;
	free_mb_ring->h = 0;
	free_mb_ring->t = N_MEM_BLOCK;
	for (i = 0; i < N_MEM_BLOCK; i++)
		free_mb_ring->v[i] = i;

	// init done_mb_ring, empty
	struct DoneMemBlockRing *done_mb_ring = &shmem.addr->done_mb_ring;
	done_mb_ring->h = 0;
	done_mb_ring->t = 0;
	done_mb_ring->t_mp = 0;

	return 0;

fail_addr:
	printk("[DERAND] create_record_ctrl(): Fail to allocate shmem.addr\n");
	return -1;
}

void delete_record_ctrl(void){
	int order;
	if (!shmem.addr)
		return;

	// find the right number of pages 
	order = get_page_order(sizeof(struct SharedMemLayout));

	// free pages
	unreserve_pages(virt_to_page(shmem.addr), 1<<order);
	free_pages((unsigned long)shmem.addr, order);    
	shmem.addr = NULL;
}
