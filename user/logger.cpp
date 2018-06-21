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
		if (logger->h < (u32)logger->t.counter){
			volatile uint8_t &ready = logger->buf[get_logger_idx(logger->h)][0];
			while (!ready); // wait for ready
			fprintf(stdout, "%s", logger->buf[get_logger_idx(logger->h)] + 1);
			ready = 0; // clear ready byte
			logger->h++;
		}
	}

	kmem.unmap_mem();
}
