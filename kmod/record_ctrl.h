#ifndef _RECORD_CTRL_H
#define _RECORD_CTRL_H

#include <net/deter.h>
#include <linux/spinlock.h>

int create_record_ctrl(void);
void delete_record_ctrl(void);

#endif /* _RECORD_CTRL_H */
