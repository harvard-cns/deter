#ifndef _KMOD__REPLAY_OPS_H
#define _KMOD__REPLAY_OPS_H

#include <linux/spinlock.h>
#include "derand_replayer.h"

struct replay_ops{
	struct derand_replayer* replayer;
	int started;
	uint32_t sip, dip;
	uint16_t sport, dport;
	struct sock *sk;
	void (*write_timer_func)(unsigned long);
	void (*delack_timer_func)(unsigned long);
	void (*keepalive_timer_func)(unsigned long);
	spinlock_t lock;
};
extern struct replay_ops replay_ops;

int replay_ops_start(struct derand_replayer *addr);
void replay_ops_stop(void);

#endif /* _KMOD__REPLAY_OPS_H */
