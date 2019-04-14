#ifndef _KMOD__MEM_BLOCK_OPS_H
#define _KMOD__MEM_BLOCK_OPS_H
#include <linux/string.h>
#include <linux/types.h>
#include "../shared_data_struct/mem_block.h"

static inline bool check_space_bit_block(struct MemBlock *b){
	return ((b->len >> 3) < MEM_BLOCK_DATA_SIZE);
}
static inline void push_bit_block(struct MemBlock *b, u8 x){ // x is 0 or 1
	u32 len = b->len;
	u32 bit_idx = len & 31;
	if (bit_idx == 0)
		((u32*)b->data)[len >> 5] = (u32)x;
	else
		((u32*)b->data)[len >> 5] |= ((u32)x) << bit_idx;
	b->len++;
}

static inline bool check_space_u8_block(struct MemBlock *b){
	return (b->len < MEM_BLOCK_DATA_SIZE);
}
static inline void push_u8_block(struct MemBlock *b, u8 x){
	b->data[b->len++] = x;
}

static inline bool check_space_u16_block(struct MemBlock *b){
	return (b->len < (MEM_BLOCK_DATA_SIZE >> 1));
}
static inline void push_u16_block(struct MemBlock *b, u16 x){
	((u16*)b->data)[b->len++] = x;
}

static inline bool check_space_u32_block(struct MemBlock *b){
	return (b->len < (MEM_BLOCK_DATA_SIZE >> 2));
}
static inline void push_u32_block(struct MemBlock *b, u32 x){
	((u32*)b->data)[b->len++] = x;
}

static inline bool check_space_u64_block(struct MemBlock *b){
	return (b->len < (MEM_BLOCK_DATA_SIZE >> 3));
}
static inline void push_u64_block(struct MemBlock *b, u64 x){
	((u64*)b->data)[b->len++] = x;
}

/*
 * for block storing nbyte-size objects
 */
// check if can put more object of nbyte size or not
static inline bool check_space_nbyte_block(struct MemBlock *b, u32 nbyte){
	return ((b->len + 1) * nbyte <= MEM_BLOCK_DATA_SIZE);
}
// push an object of size nbyte. addr is the address of the object
static inline void push_nbyte_block(struct MemBlock *b, u32 nbyte, void* addr){
	memcpy(b->data + b->len * nbyte, addr, nbyte);
	b->len++;
}

#endif /* _KMOD__MEM_BLOCK_OPS_H */
