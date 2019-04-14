#ifndef _SHARED_DATA_STRUCT__MEM_BLOCK_H
#define _SHARED_DATA_STRUCT__MEM_BLOCK_H

/* NOTE: MEM_BLOCK_SIZE * 8 must < 65536, because u16 len can be number of bits */
#define MEM_BLOCK_SIZE 1024
#define MEM_BLOCK_HDR_SIZE 8
#define MEM_BLOCK_DATA_SIZE (MEM_BLOCK_SIZE - MEM_BLOCK_HDR_SIZE)

struct MemBlock{
	union{
		struct {
			u16 len;
			u8 type; // type of data this block stores.
			u32 rec_id; // index of the recorder this MemBlock belongs to
		};
		u8 head[MEM_BLOCK_HDR_SIZE];
	};
	u8 data[MEM_BLOCK_DATA_SIZE];
};

#endif /* _SHARED_DATA_STRUCT__MEM_BLOCK_H */
