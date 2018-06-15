#include "replay_ctrl.h"
#include "mem_util.h"
#include "derand_replayer.h"

struct replay_ctrl replay_ctrl = {
	.addr = NULL,
	.size = 0,
};

static void initialize_replay(void){
	int i;
	struct derand_replayer *r = (struct derand_replayer*) replay_ctrl.addr;

	// initialize evtq
	r->evtq.h = 0;

	// initialize jfq
	r->jfq.h = 0;
	if (r->jfq.t > 0)
		r->jfq.last_jiffies = r->jfq.v[0].init_jiffies;
	r->jfq.idx_delta = 0;

	// initialize mpq
	r->mpq.h = 0;
	r->mpq.seq = 0;

	// initialize maq
	r->maq.h = 0;
	if (r->maq.t > 0)
		r->maq.last_v = r->maq.v[0].init_v;
	r->maq.idx_delta = 0;

	// initialize msq
	r->msq.h= 0;

	// initialize ebq
	for (i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++)
		r->ebq[i].h = 0;

	printk("%u %u %u %u %u\n", r->evtq.t, r->jfq.t, r->mpq.t, r->maq.t, r->msq.t);
}
/* Main replay control function */
static void replay_start(void){
	// initialize data
	initialize_replay();
	// start kthread
}

// struct name: proc_derand_replay_expose
INIT_PROC_EXPOSE(derand_replay)

/* proc read callback:
 * This function returns the shared memory address */
static int expose_addr(void *args, char* buf, size_t len){
	return sprintf(buf, "0x%llx\n", virt_to_phys(replay_ctrl.addr));
}

/* proc write callback: 
 * This function should be called when the user finish copy data */
static int user_copy_finish(void *args, char* buf, size_t len){
	// if this call conforms to the protocol
	if (strcmp(buf, "copy finish") == 0){
		replay_start();
		return 1;
	}
	return -1;
}

/* proc write callback:
 * This function should be called when user tells the buffer size.
 * Allocate enough memory, and share with the user.
 * Set the read callback pointer to expose_addr (so expose the address to user).
 * Change the write callback pointer to user_copy_finish (so user can tell us copy finish) */
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
	
	// set the next input callback for user copy finish
	proc_derand_replay_expose.input_func = user_copy_finish;

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
