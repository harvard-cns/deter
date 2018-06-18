#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/socket.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/tcp.h>

#include "replay_ctrl.h"
#include "logger.h"

static int __init replay_init(void)
{
	replay_prepare();
	init_logger();
	return 0;
}

static void __exit replay_exit(void)
{
	replay_stop();
	clear_logger();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuliang Li");
MODULE_DESCRIPTION("derand replayer");

module_init(replay_init);
module_exit(replay_exit);
