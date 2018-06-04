#include "proc_expose.h"

int proc_expose_start(struct proc_expose *expose){
	if (!proc_create(expose->proc_file_name, 0x666, NULL, &expose->fops))
		return -1;
	return 0;
}

void proc_expose_stop(struct proc_expose *expose){
	remove_proc_entry(expose->proc_file_name, NULL);
}
