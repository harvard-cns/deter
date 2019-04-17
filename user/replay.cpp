#include <cstdio>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "mem_share.hpp"
#include "replayer.hpp"

using namespace std;

static int write_proc(const string &proc_file_name, char *buf, uint32_t len){
	int ret = 0;

	// open proc file
	int fd = open(("/proc/" + proc_file_name).c_str(), O_WRONLY);
	if (fd == -1){
		fprintf(stderr, "Fail to open /proc/%s\n", proc_file_name.c_str());
		return -1;
	}

	// write to proc file
	ret = write(fd, buf, len);
	close(fd);

	if (ret == 1){ // succeed
		return 0;
	}else{
		fprintf(stderr, "Fail to write proc\n");
		return -2;
	}
}

static int send_buffer_size(const string &proc_file_name){
	char buf[32];
	sprintf(buf, "%lu", sizeof(DeterReplayer));
	if (write_proc(proc_file_name, buf, strlen(buf))){
		fprintf(stderr, "Fail to send buffer_size\n");
		return -1;
	}
	return 0;
}

static int send_copy_finish(const string &proc_file_name){
	// This is the protocal: user sends "copy finish" to indicate copy finish
	char buf[] = "copy finish";
	if (write_proc(proc_file_name, buf, strlen(buf))){
		fprintf(stderr, "Fail to send copy finish\n");
		return -1;
	}
	return 0;
}

int main(int argc, char **argv){
	if (argc != 2 && argc != 3){
		fprintf(stderr, "Usage: ./replay <records> [<dstip>]\n");
		return -1;
	}

	string dip = "";
	if (argc == 3)
		dip = argv[2];

	// setup shared memory
	send_buffer_size("deter_replay");
	KernelMem kmem;
	if (kmem.map_proc_exposed_mem("deter_replay", sizeof(DeterReplayer)))
		return -1;

	// make replayer
	Replayer r;
	r.set_addr(kmem.buf);
	if (r.read_records(argv[1]))
		goto fail_read_records;

	// ensure we enter dip for client mode
	if (r.rec.mode == 1 && dip == ""){
		fprintf(stderr, "Please enter dstip for client replay\n");
		goto no_dip;
	}

	// tell kernel copy finish
	send_copy_finish("deter_replay");

	// start replay
	if (r.rec.mode == 0){
		if (r.start_replay_server())
			fprintf(stderr, "server socket setup fail\n");
	}else {
		if (r.start_replay_client(dip))
			fprintf(stderr, "client socket setup fail\n");
	}

no_dip:
fail_read_records:
	kmem.unmap_mem();
	return 0;
}
