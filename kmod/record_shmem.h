#ifndef _KMOD__RECORD_SHMEM_H
#define _KMOD__RECORD_SHMEM_H

#include "deter_recorder.h"

/* This is the memory pool for all deter data. 
 * addr is by default NULL, and initalized by deter kernel module */
struct record_shmem{
	struct SharedMemLayout *addr;
};
extern struct record_shmem shmem;

#endif /* _KMOD__RECORD_SHMEM_H */
