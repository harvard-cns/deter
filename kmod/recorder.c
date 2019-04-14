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
#include "logger.h"

u64 dstip;
u64 ndstip;
module_param(dstip, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(dstip, "A dstip to monitor");
module_param(ndstip, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(ndstip, "A dstip NOT to monitor");

static int __init record_init(void)
{
	printk("dstip to monitor: 0x%08lx\n", dstip);
	printk("dstip NOT to monitor: 0x%08lx\n", ndstip);
	mon_dstip = htonl(dstip); // mon_dstip is in record_ops.c
	mon_ndstip = htonl(ndstip);

	// create record_ctrl data
	if (create_record_ctrl())
		goto fail_create_ctrl;

	// expose data to user space
	if (share_mem_to_user())
		goto fail_share;

	// bind record_ops to kernel stack
	if (bind_record_ops())
		goto fail_bind_ops;

	// init logger
	init_logger();
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

	// clear logger
	clear_logger();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuliang Li");
MODULE_DESCRIPTION("derand recorder");

module_init(record_init);
module_exit(record_exit);
