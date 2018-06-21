#ifndef _SHARED_DATA_STRUCT__LOGGER_H
#define _SHARED_DATA_STRUCT__LOGGER_H

#define LOGGER_MAX_MSG 256
#define LOGGER_N_MSG 128
struct Logger{
	volatile u32 h;
	atomic_t t;
	u8 buf[LOGGER_N_MSG][LOGGER_MAX_MSG];
	volatile u32 running;
};
#define get_logger_idx(i) ((i) & (LOGGER_N_MSG - 1))

#endif /* _SHARED_DATA_STRUCT__LOGGER_H */
