#include "record_user_share.h"
#include "record_shmem.h"
#include "proc_expose.h"

// struct name: proc_deter_expose
INIT_PROC_EXPOSE(deter)

// function for output_func
static int expose_addr(void *args, char* buf, size_t len){
	return sprintf(buf, "0x%llx\n", virt_to_phys(shmem.addr));
}

int share_mem_to_user(void){
	int ret;
	proc_deter_expose.output_func = expose_addr;
	ret = proc_expose_start(&proc_deter_expose);
	if (ret){
		printk("[DETER] share_mem_to_user: Fail to open proc file\n");
		return -1;
	}
	return 0;
}

void stop_share_mem_to_user(void){
	proc_expose_stop(&proc_deter_expose);
}
