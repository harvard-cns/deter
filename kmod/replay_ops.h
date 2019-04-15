#ifndef _KMOD__REPLAY_OPS_H
#define _KMOD__REPLAY_OPS_H

#include <linux/spinlock.h>
#include "deter_replayer.h"

enum ReplayState{
	NOT_STARTED = 0,
	STARTED = 1,
	SHOULD_STOP = 2,
	STOPPED = 3,
};

struct replay_ops{
	struct DeterReplayer* replayer;
	volatile enum ReplayState state;
	uint32_t sip, dip;
	uint16_t sport, dport;
	struct sock *sk;
	void (*write_timer_func)(unsigned long);
	void (*delack_timer_func)(unsigned long);
	void (*keepalive_timer_func)(unsigned long);
	spinlock_t lock;
};
extern struct replay_ops replay_ops;

int replay_ops_start(struct DeterReplayer *addr);
void replay_ops_stop(void);

#endif /* _KMOD__REPLAY_OPS_H */
