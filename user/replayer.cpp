#include <cstdio>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "derand_replayer.h"

using namespace std;

static int write_to_proc(const string &proc_file_name){
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
	printf("%d\n", ret);

	// close the proc file
	close(fd);
	return ret;
}

int main(int argc, char **argv){
	write_to_proc("derand_replay");
	return 0;
}
