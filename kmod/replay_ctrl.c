#include "replay_ctrl.h"
#include "mem_util.h"

struct replay_ctrl replay_ctrl = {
	.addr = NULL,
	.size = 0,
};

// struct name: proc_derand_replay_expose
INIT_PROC_EXPOSE(derand_replay)

static int expose_addr(void *args, char* buf, size_t len){
	return sprintf(buf, "0x%llx\n", virt_to_phys(replay_ctrl.addr));
}

/* Callback function when user tells the buffer size.
 * Allocate enough memory, and share with the user */
static int user_input_buffer_size(void *args, char* buf, size_t len){
	int order;
	if (replay_ctrl.addr != NULL){
		printk("[DERAND_REPLAY] Warn: cannot accept user buffer_size more than once\n");
		return -1;
	}
	if (sscanf(buf, "%u", &replay_ctrl.size) == 0){
		printk("[DERAND_REPLAY] Error: user input buffer_size format error\n");
		return -1;
	}
	printk("[DERAND_REPLAY] buffer size = %u\n", replay_ctrl.size);

	// allocate memory
	order = get_page_order(replay_ctrl.size);
	replay_ctrl.addr = (void*)__get_free_pages(GFP_KERNEL, order);
	if (!replay_ctrl.addr){
		printk("[DERAND_REPLAY] fail get free pages\n");
		return -1;
	}
	printk("[DERAND_REPLAY] successfully get %d pages\n", 1 << order);

	// reserve pages
	reserve_pages(virt_to_page(replay_ctrl.addr), 1<<order);

	// expose the addr to user
	proc_derand_replay_expose.output_func = expose_addr;
	return 1;
}

int replay_prepare(void){
	int ret;
	proc_derand_replay_expose.input_func = user_input_buffer_size;
	ret = proc_expose_start(&proc_derand_replay_expose);
	if (ret){
		printk("[DERAND_REPLAY] replay_prepare: Fail to open proc file\n");
		return -1;
	}
	return 0;
}

void replay_stop(void){
	int order;

	// close proc file
	proc_expose_stop(&proc_derand_replay_expose);

	// reclaim memory
	if (replay_ctrl.addr){ 
		order = get_page_order(replay_ctrl.size);
		unreserve_pages(virt_to_page(replay_ctrl.addr), 1 << order);
		free_pages((unsigned long)replay_ctrl.addr, order);    
		replay_ctrl.addr = NULL;
	}
}
