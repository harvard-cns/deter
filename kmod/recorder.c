#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/socket.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/tcp.h>

#include "record_ctrl.h"
#include "record_ops.h"
#include "record_user_share.h"

static int __init record_init(void)
{
	// create record_ctrl data
	if (create_record_ctrl(4))
		goto fail_create_ctrl;

	// expose data to user space
	if (share_mem_to_user())
		goto fail_share;

	// bind record_ops to kernel stack
	if (bind_record_ops())
		goto fail_bind_ops;
	return 0;
	
fail_bind_ops:
	stop_share_mem_to_user();
fail_share:
	delete_record_ctrl();
fail_create_ctrl:
	return -1;
}

static void __exit record_exit(void)
{
	// unbind record_ops
	unbind_record_ops();

	// stop exposing data to user space
	stop_share_mem_to_user();
	
	// remove record_ctrl data
	delete_record_ctrl();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuliang Li");
MODULE_DESCRIPTION("derand recorder");

module_init(record_init);
module_exit(record_exit);
