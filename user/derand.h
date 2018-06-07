#ifndef _DERAND_H
#define _DERAND_H

#include <stdint.h>
struct atomic_t {
	int counter;
};
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#include "../shared_data_struct/derand_recorder.h"

#endif /* _NET_DERAND_H */
