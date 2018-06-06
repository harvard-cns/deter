#include <cstdio>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "mem_share.hpp"

using namespace std;

static int read_phy_addr_from_proc(const string &proc_file_name, unsigned long &res){
	int ret = 0;

	// open proc file
	int fd = open(("/proc/" + proc_file_name).c_str(), O_RDONLY);
	if (fd == -1){
		fprintf(stderr, "Fail to open /proc/%s\n", proc_file_name.c_str());
		ret = -1;
		goto fail_open_proc;
	}

	// read addr from proc file
	char s[256];
	if (read(fd, s, sizeof(s)) == -1){
		fprintf(stderr, "Fail to read from /proc/%s\n", proc_file_name.c_str());
		ret = -2;
		goto fail_read;
	}
	if (sscanf(s, "%lx", &res) == EOF){
		fprintf(stderr, "Format error in /proc/%s\n", proc_file_name.c_str());
		ret = -3;
		goto fail_read;
	}

fail_read:
	// close the proc file
	close(fd);
fail_open_proc:
	return ret;
}

int KernelMem::map_proc_exposed_mem(const string &proc_file_name, uint32_t mem_range){
	if (read_phy_addr_from_proc(proc_file_name, phy_addr))
		return -1;
	
	// mmap shared memory
	int fd = open("/dev/mem", O_RDWR);
	if (fd == -1){
		fprintf(stderr, "Fail to open /dev/mem\n");
		return -2;
	}

	buf = mmap(0, mem_range, PROT_READ|PROT_WRITE, MAP_SHARED, fd, phy_addr);
	if (buf == MAP_FAILED){
		fprintf(stderr, "Fail to mmap\n");
		return -3;
	}

	range = mem_range;

	return 0;
}

void KernelMem::unmap_mem(){
	munmap(buf, range);
}
