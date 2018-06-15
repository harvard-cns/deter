#ifndef _DERAND_REPLAYER_H
#define _DERAND_REPLAYER_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
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

#include "../shared_data_struct/derand_replayer.h"

#endif /* _DERAND_REPLAYER_H */

