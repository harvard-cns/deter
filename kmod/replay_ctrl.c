#include "replay_ctrl.h"
#include "mem_util.h"
#include "deter_replayer.h"
#include "replay_ops.h"
#include "logger.h"

struct replay_ctrl replay_ctrl = {
	.addr = NULL,
	.size = 0,
	.replay_started = false,
};

// struct name: proc_deter_replay_expose
INIT_PROC_EXPOSE(deter_replay)

/********************************************
 * start of replay:
 * code logic:
 *   first expose a proc file for user to write the buffer size (user_input_buffer_size)
 *   then allocate the buffer, expose its address to user throught the proc file (expose_addr)
 *   Then user will write the proc file again to tell us it has finished copying data (user_copy_finish), so we can start replay.
 * So user_copy_finish() is the real start point of replay.
 *******************************************/
/* proc read callback:
 * This function returns the shared memory address */
static int expose_addr(void *args, char* buf, size_t len){
	return sprintf(buf, "0x%llx\n", virt_to_phys(replay_ctrl.addr));
}

/* proc write callback: 
 * This function should be called when the user finish copy data */
static int user_copy_finish(void *args, char* buf, size_t len){
	// if this call conforms to the protocol
	if (strcmp(buf, "copy finish") != 0)
		return -1;
	// start the replay
	if (replay_ops_start(replay_ctrl.addr))
		return -1;

	replay_ctrl.replay_started = true;
	return 1;
}

/* proc write callback:
 * This function should be called when user tells the buffer size.
 * Allocate enough memory, and share with the user.
 * Set the read callback pointer to expose_addr (so expose the address to user).
 * Change the write callback pointer to user_copy_finish (so user can tell us copy finish) */
static int user_input_buffer_size(void *args, char* buf, size_t len){
	int order;
	if (replay_ctrl.addr != NULL){
		deter_log("Warning: cannot accept user buffer_size more than once\n");
		return -1;
	}
	if (sscanf(buf, "%u", &replay_ctrl.size) == 0){
		deter_log("Error: user input buffer_size format error\n");
		return -1;
	}
	deter_log("buffer size = %u\n", replay_ctrl.size);

	// allocate memory
	order = get_page_order(replay_ctrl.size);
	replay_ctrl.addr = (void*)__get_free_pages(GFP_KERNEL, order);
	if (!replay_ctrl.addr){
		deter_log("fail get free pages\n");
		return -1;
	}
	deter_log("successfully get %d pages\n", 1 << order);

	// reserve pages
	reserve_pages(virt_to_page(replay_ctrl.addr), 1<<order);

	// expose the addr to user
	proc_deter_replay_expose.output_func = expose_addr;
	
	// set the next input callback for user copy finish
	proc_deter_replay_expose.input_func = user_copy_finish;

	return 1;
}

int replay_prepare(void){
	int ret;
	proc_deter_replay_expose.input_func = user_input_buffer_size;
	ret = proc_expose_start(&proc_deter_replay_expose);
	if (ret){
		deter_log("replay_prepare: Fail to open proc file\n");
		return -1;
	}
	return 0;
}

/********************************************
 * finish of replay
 *******************************************/
void replay_stop(void){
	int order;

	// stop replay_ops
	if (replay_ctrl.replay_started){
		replay_ops_stop();
		replay_ctrl.replay_started = false;
	}

	// close proc file
	proc_expose_stop(&proc_deter_replay_expose);

	// reclaim memory
	if (replay_ctrl.addr){ 
		order = get_page_order(replay_ctrl.size);
		unreserve_pages(virt_to_page(replay_ctrl.addr), 1 << order);
		free_pages((unsigned long)replay_ctrl.addr, order);    
		replay_ctrl.addr = NULL;
	}
}
