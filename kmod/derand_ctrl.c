#include <linux/slab.h>
#include <linux/mm.h>
#include "derand_ctrl.h"
#include "derand_recorder.h"

struct derand_ctrl derand_ctrl = {
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
int create_derand_ctrl(int max_sock){
	int i;

	// find the right number of pages 
	int order = get_page_order(max_sock * sizeof(struct derand_recorder));

	// allocate and reserve pages
	derand_ctrl.addr = (struct derand_recorder*)__get_free_pages(GFP_KERNEL, order);
	if (derand_ctrl.addr){
		reserve_pages(virt_to_page(derand_ctrl.addr), 1<<order);
		// mark not in use for all recorders
		for (i = 0; i < max_sock; i++)
			derand_ctrl.addr[i].recorder_id = -1;
	}else 
		goto fail_addr;

	// allocate memory for stack
	derand_ctrl.stack = kmalloc(sizeof(int) * max_sock, GFP_KERNEL);
	if (!derand_ctrl.stack)
		goto fail_stack;

	// init stack of free memory for sockets
	for (i = 0; i < max_sock; i++)
		derand_ctrl.stack[i] = max_sock - i - 1;
	derand_ctrl.top = max_sock;

	// set max_sock
	derand_ctrl.max_sock = max_sock;

	return 0;

fail_stack:
	unreserve_pages(virt_to_page(derand_ctrl.addr), 1<<order);
	free_pages((unsigned long)derand_ctrl.addr, order);    
	derand_ctrl.addr = NULL;
	printk("[DERAND] create_derand_ctrl(): Fail to allocate derand_ctrl.stack\n");
fail_addr:
	printk("[DERAND] create_derand_ctrl(): Fail to allocate derand_ctrl.addr\n");
	return -1;
}

void delete_derand_ctrl(void){
	int order;
	if (!derand_ctrl.addr)
		return;

	// find the right number of pages 
	order = get_page_order(derand_ctrl.max_sock * sizeof(struct derand_recorder));

	// free pages
	unreserve_pages(virt_to_page(derand_ctrl.addr), 1<<order);
	free_pages((unsigned long)derand_ctrl.addr, order);    
	derand_ctrl.addr = NULL;
	// free stack
	kfree(derand_ctrl.stack);
	// clear stack top
	derand_ctrl.top = 0;
	// clear max_sock
	derand_ctrl.max_sock = 0;
}
