#ifndef _KMOD__REPLAY_OPS_H
#define _KMOD__REPLAY_OPS_H

#include <linux/spinlock.h>
#include "derand_replayer.h"

struct replay_ops{
	struct derand_replayer* replayer;
	int started;
	uint16_t sport, dport;
	spinlock_t lock;
};
extern struct replay_ops replay_ops;

int replay_kthread(void *args);

void bind_replay_ops(void);
void unbind_replay_ops(void);

#endif /* _KMOD__REPLAY_OPS_H */
