#include "logger.h"
#include "mem_util.h"
#include "proc_expose.h"

// struct name: proc_derand_logger_expose
INIT_PROC_EXPOSE(derand_logger)

struct Logger *logger = NULL;
spinlock_t logger_lock = __SPIN_LOCK_UNLOCKED();

// function for output_func
static int expose_addr(void *args, char* buf, size_t len){
	return sprintf(buf, "0x%llx\n", virt_to_phys(logger));
}

int init_logger(void){
	int ret;
	int order = get_page_order(sizeof(struct Logger));

	spin_lock(&logger_lock);
	logger = (struct Logger*) __get_free_pages(GFP_KERNEL, order);
	if (logger){
		reserve_pages(virt_to_page(logger), 1<<order);
	}else {
		printk("[DERAND] Fail to allocate memory for logger\n");
		spin_unlock(&logger_lock);
		return -1;
	}

	// init logger
	memset(logger, 0, sizeof(struct Logger));
	logger->h = 0;
	atomic_set(&logger->t, 0);
	logger->running = 1;
	printk("logger->running = %u\n", logger->running);
	spin_unlock(&logger_lock);

	// expose through proc
	proc_derand_logger_expose.output_func = expose_addr;
	ret = proc_expose_start(&proc_derand_logger_expose);
	if (ret){
		printk("[DERAND] Fail to open proc file for logger\n");
		return -1;
	}
	return 0;
}

void clear_logger(void){
	int order;
	proc_expose_stop(&proc_derand_logger_expose);

	spin_lock(&logger_lock);
	logger->running = 0;
	order = get_page_order(sizeof(struct Logger));
	unreserve_pages(virt_to_page(logger), 1<<order);
	free_pages((unsigned long)logger, order);    
	logger = NULL;
	spin_unlock(&logger_lock);
}

int derand_log(const char *fmt, ...){
	int ret = 0;
	u32 idx;
	va_list args;
	va_start(args, fmt);
	//spin_lock(&logger_lock);
	if (logger != NULL){
		while ((u32)atomic_read(&logger->t) - logger->h >= LOGGER_N_MSG); // wait for available space
		idx = atomic_add_return(1, &logger->t) - 1;
		ret = vsprintf(logger->buf[get_logger_idx(idx)] + 1, fmt, args); // the first char is used to indicate if the buffer is ready
		logger->buf[get_logger_idx(idx)][0] = 1; // mark as ready
		//logger->t++;
	}
	//spin_unlock(&logger_lock);
	return ret;
}
