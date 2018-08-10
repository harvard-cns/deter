#ifndef _KMOD__LOGGER_H
#define _KMOD__LOGGER_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include "../shared_data_struct/logger.h"

extern struct Logger *logger;
extern spinlock_t logger_lock;

int init_logger(void);
void clear_logger(void);
int derand_log_va(const char *fmt, va_list args);
int derand_log(const char *fmt, ...);

#endif /* _KMOD__LOGGER_H */
