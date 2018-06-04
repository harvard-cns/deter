#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/socket.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/tcp.h>

#include "derand_ctrl.h"
#include "derand_ops.h"
#include "derand_user_share.h"

static int __init derand_init(void)
{
	// create derand_ctrl data
	if (create_derand_ctrl(10))
		goto fail_create_ctrl;

	// expose data to user space
	if (share_mem_to_user())
		goto fail_share;

	// bind derand_ops to kernel stack
	if (bind_derand_ops())
		goto fail_bind_ops;
	return 0;
	
fail_bind_ops:
	stop_share_mem_to_user();
fail_share:
	delete_derand_ctrl();
fail_create_ctrl:
	return -1;
}

static void __exit derand_exit(void)
{
	return;
	// unbind derand_ops
	unbind_derand_ops();

	// stop exposing data to user space
	stop_share_mem_to_user();
	
	// remove derand_ctrl data
	delete_derand_ctrl();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuliang Li");
MODULE_DESCRIPTION("derand");

module_init(derand_init);
module_exit(derand_exit);
