#include "kernel_typedef.hpp"
#include "../shared_data_struct/logger.h"
#include "mem_share.hpp"

#define LOGGER_PROC_NAME "derand_logger"

int main(){
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem(LOGGER_PROC_NAME, sizeof(Logger)))
		return -1;

	Logger *logger = (Logger*)kmem.buf;

	while (logger->running){
		if (logger->h < logger->t){
			fprintf(stdout, "%s", logger->buf[get_logger_idx(logger->h)]);
			logger->h++;
		}
	}

	kmem.unmap_mem();
}
