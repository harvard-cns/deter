#ifndef _MEM_SHARE_HPP
#define _MEM_SHARE_HPP

#include <string>

class KernelMem{
public:
	unsigned long phy_addr;
	void *buf;
	uint32_t range;

	int map_proc_exposed_mem(const std::string &proc_file_name, uint32_t mem_range);
	void unmap_mem();
};

#endif /* _MEM_SHARE_HPP */
