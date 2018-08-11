#include <vector>
#include <string>
#include <thread>
#include <signal.h>
#include "kernel_typedef.hpp"
#include "../shared_data_struct/logger.h"
#include "mem_share.hpp"

using namespace std;

#define LOGGER_PROC_NAME "derand_logger"

vector<string> logs;

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

volatile bool force_quit = false;
static void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

int main(){
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem(LOGGER_PROC_NAME, sizeof(Logger)))
		return -1;

	Logger *logger = (Logger*)kmem.buf;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	//thread th(test_thread, logger);
	while (logger->running && !force_quit){
		if (logger->h < (u32)logger->t.counter){
			volatile uint8_t &ready = logger->buf[get_logger_idx(logger->h)][0];
			for (uint32_t cnt = 1; !ready; cnt++){ // wait for ready
				if ((cnt & 0x0fffffff) == 0)
					printf("waiting for ready of entry[%u]\n", get_logger_idx(logger->h));
			}
			fprintf(stdout, "[%u %u] %s", logger->h, logger->t.counter, logger->buf[get_logger_idx(logger->h)] + 1);
			//logs.push_back((char*)logger->buf[get_logger_idx(logger->h)] + 1);
			ready = 0; // clear ready byte
			logger->h++;
		}
	}
	for (int i = 0; i < logs.size(); i++){
		fprintf(stdout, "%s", logs[i].c_str());
	}

	kmem.unmap_mem();
}
