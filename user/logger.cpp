#include <thread>
#include "kernel_typedef.hpp"
#include "../shared_data_struct/logger.h"
#include "mem_share.hpp"

using namespace std;

#define LOGGER_PROC_NAME "derand_logger"

void test_thread(Logger *logger){
	char buf[256];
	while (logger->running){
		scanf("%s", buf);
		printf("read from cmd: %s, h=%u t=%u\n", buf, logger->h, (u32)logger->t.counter);
		if (!logger->running){
			printf("logger stop running\n");
			break;
		}
		u32 t = logger->t.counter++;
		sprintf((char*)logger->buf[get_logger_idx(t)]+1, "%s\n", buf);
		logger->buf[get_logger_idx(t)][0] = 1;
	}
}

int main(){
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem(LOGGER_PROC_NAME, sizeof(Logger)))
		return -1;

	Logger *logger = (Logger*)kmem.buf;

	thread th(test_thread, logger);
	while (logger->running){
		if (logger->h < (u32)logger->t.counter){
			volatile uint8_t &ready = logger->buf[get_logger_idx(logger->h)][0];
			while (!ready); // wait for ready
			fprintf(stdout, "[%u %u] %s", logger->h, logger->t.counter, logger->buf[get_logger_idx(logger->h)] + 1);
			ready = 0; // clear ready byte
			logger->h++;
		}
	}

	kmem.unmap_mem();
}
