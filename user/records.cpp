#include <arpa/inet.h>
#include "records.hpp"

using namespace std;

int Records::dump(const char* filename){
	int ret;
	uint32_t len;
	FILE* fout;
	if (filename == NULL){
		char buf[128];
		sprintf(buf, "%08x:%hu->%08x:%hu", ntohl(sip), ntohs(sport), ntohl(dip), ntohs(dport));
		fout = fopen(buf, "w");
	}else 
		fout = fopen(filename, "w");

	// write 4 tuples
	if (!fwrite(&sip, sizeof(sip)+sizeof(dip)+sizeof(sport)+sizeof(dport), 1, fout))
		goto fail_write;

	// write events
	len = evts.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&evts[0], sizeof(derand_event) * len, 1, fout))
		goto fail_write;

	// write sockcalls
	len = sockcalls.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&sockcalls[0], sizeof(derand_rec_sockcall) * len, 1, fout))
		goto fail_write;

	fclose(fout);
	return 0;
fail_write:
	fclose(fout);
	return -1;
}

void Records::clear(){
	evts.clear();
	sockcalls.clear();
}
