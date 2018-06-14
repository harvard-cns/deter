#ifndef _RECORD_CTRL_H
#define _RECORD_CTRL_H

#include <net/derand.h>
#include <linux/spinlock.h>
#include "derand_recorder.h"

/* This is the memory pool for all derand data. 
 * addr is by default NULL, and initalized by derand kernel module */
struct record_ctrl{
	struct derand_recorder* addr;
	int max_sock; // max number of sockets for record
	int *stack; // stack for usable recorder memory
	int top;
	spinlock_t lock;
};
extern struct record_ctrl record_ctrl;

int create_record_ctrl(int max_sock);
void delete_record_ctrl(void);

#endif /* _RECORD_CTRL_H */
