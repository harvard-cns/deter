#ifndef _USER__KERNEL_TYPEDEF_HPP
#define _USER__KERNEL_TYPEDEF_HPP
#include <stdint.h>

typedef uint8_t u8;
typedef uint8_t __u8;
typedef uint16_t u16;
typedef uint16_t __u16;
typedef uint32_t u32;
typedef uint32_t __u32;
typedef uint64_t u64;
typedef int32_t s32;

struct atomic_t {
	int counter;
};

struct skb_mstamp {
	union {
		u64		v64;
		struct {
			u32	stamp_us;
			u32	stamp_jiffies;
		};
	};
};

#endif /* _USER__KERNEL_TYPEDEF_HPP */
