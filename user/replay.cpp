#include <cstdio>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "mem_share.hpp"
#include "replayer.hpp"

using namespace std;

static int send_buffer_size(const string &proc_file_name){
	int ret = 0;

	// open proc file
	int fd = open(("/proc/" + proc_file_name).c_str(), O_WRONLY);
	if (fd == -1){
		fprintf(stderr, "Fail to open /proc/%s\n", proc_file_name.c_str());
		return -1;
	}

	// write to proc file
	char buf[32];
	sprintf(buf, "%lu", sizeof(derand_replayer));
	ret = write(fd, buf, strlen(buf));
	close(fd);

	if (ret == 1){ // succeed
		return 0;
	}else{
		fprintf(stderr, "Fail to send kernel the buffer size\n");
		return -2;
	}
}

int main(int argc, char **argv){
	if (argc != 2){
		fprintf(stderr, "Usage: ./replay <records>\n");
		return -1;
	}
	send_buffer_size("derand_replay");
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem("derand_replay", sizeof(derand_replayer)))
		return -1;

	// read records
	Records rec;
	rec.read(argv[1]);

	// make replayer

	kmem.unmap_mem();
	return 0;
}
