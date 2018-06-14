#include <linux/slab.h>
#include <linux/mm.h>
#include "record_ctrl.h"
#include "derand_recorder.h"

struct record_ctrl record_ctrl = {
	.addr = NULL,
	.max_sock = 0,
	.stack = NULL,
	.top = 0,
	.lock = __SPIN_LOCK_UNLOCKED()
};

int get_page_order(int n){
	int order = 0;
	while ((1<<order) * 4096 < n)
		order++;
	return order;
}
void reserve_pages(struct page* pg, int n_page){
	struct page *end = pg + n_page;
	for (; pg < end; pg++)
		SetPageReserved(pg);
}
void unreserve_pages(struct page* pg, int n_page){
	struct page *end = pg + n_page;
	for (; pg < end; pg++)
		ClearPageReserved(pg);
}
int create_record_ctrl(int max_sock){
	int i;

	// find the right number of pages 
	int order = get_page_order(max_sock * sizeof(struct derand_recorder));

	// allocate and reserve pages
	record_ctrl.addr = (struct derand_recorder*)__get_free_pages(GFP_KERNEL, order);
	if (record_ctrl.addr){
		reserve_pages(virt_to_page(record_ctrl.addr), 1<<order);
		// mark not in use for all recorders
		for (i = 0; i < max_sock; i++)
			record_ctrl.addr[i].recorder_id = -1;
	}else 
		goto fail_addr;

	// allocate memory for stack
	record_ctrl.stack = kmalloc(sizeof(int) * max_sock, GFP_KERNEL);
	if (!record_ctrl.stack)
		goto fail_stack;

	// init stack of free memory for sockets
	for (i = 0; i < max_sock; i++)
		record_ctrl.stack[i] = max_sock - i - 1;
	record_ctrl.top = max_sock;

	// set max_sock
	record_ctrl.max_sock = max_sock;

	return 0;

fail_stack:
	unreserve_pages(virt_to_page(record_ctrl.addr), 1<<order);
	free_pages((unsigned long)record_ctrl.addr, order);    
	record_ctrl.addr = NULL;
	printk("[DERAND] create_record_ctrl(): Fail to allocate record_ctrl.stack\n");
fail_addr:
	printk("[DERAND] create_record_ctrl(): Fail to allocate record_ctrl.addr\n");
	return -1;
}

void delete_record_ctrl(void){
	int order;
	if (!record_ctrl.addr)
		return;

	// find the right number of pages 
	order = get_page_order(record_ctrl.max_sock * sizeof(struct derand_recorder));

	// free pages
	unreserve_pages(virt_to_page(record_ctrl.addr), 1<<order);
	free_pages((unsigned long)record_ctrl.addr, order);    
	record_ctrl.addr = NULL;
	// free stack
	kfree(record_ctrl.stack);
	// clear stack top
	record_ctrl.top = 0;
	// clear max_sock
	record_ctrl.max_sock = 0;
}
