#ifndef _KMOD__UTILS_H
#define _KMOD__UTILS_H
#include <asm/atomic.h>
static inline u32 atomic_read_u32(u32 *x){
	return (u32)atomic_read((atomic_t*)x);
}
static inline void atomic_inc_u32(u32 *x){
	atomic_inc((atomic_t*)x);
}
#endif /* _KMOD__UTILS_H */
