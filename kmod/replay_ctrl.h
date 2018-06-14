#ifndef _REPLAY_CTRL_H
#define _REPLAY_CTRL_H

#include "proc_expose.h"

struct replay_ctrl{
	void *addr;
	u32 size;
};
extern struct replay_ctrl replay_ctrl;

/* expose proc file for user space to input buffer size*/
int replay_prepare(void);

void replay_stop(void);

#endif /* _REPLAY_CTRL_H */
