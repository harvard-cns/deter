#include "records.hpp"

using namespace std;

int Records::dump(const char* filename){
	int ret;
	uint32_t len;
	FILE* fout;
	if (filename == NULL){
		char buf[128];
		sprintf(buf, "%08x:%hu->%08x:%hu", sip, sport, dip, dport);
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

	// write jiffies_reads
	len = jiffies_reads.size();
	if (!fwrite(&len, sizeof(len), 1, fout))
		goto fail_write;
	if (!fwrite(&jiffies_reads[0], sizeof(jiffies_read) * len, 1, fout))
		goto fail_write;

	fclose(fout);
	return 0;
fail_write:
	fclose(fout);
	return -1;
}

int Records::read(const char* filename){
	FILE* fin= fopen(filename, "r");
	uint32_t len;
	// write 4 tuples
	if (!fread(&sip, sizeof(sip)+sizeof(dip)+sizeof(sport)+sizeof(dport), 1, fin))
		goto fail_read;

	// read events
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	evts.resize(len);
	if (!fread(&evts[0], sizeof(derand_event) * len, 1, fin))
		goto fail_read;

	// read sockcalls
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	sockcalls.resize(len);
	if (!fread(&sockcalls[0], sizeof(derand_rec_sockcall) * len, 1, fin))
		goto fail_read;

	// read jiffies_reads
	if (!fread(&len, sizeof(len), 1, fin))
		goto fail_read;
	jiffies_reads.resize(len);
	if (!fread(&jiffies_reads[0], sizeof(jiffies_read) * len, 1, fin))
		goto fail_read;

	fclose(fin);
	return 0;
fail_read:
	fclose(fin);
	return -1;
}

void Records::print(FILE* fout){
	fprintf(fout, "%08x:%hu %08x:%hu\n", sip, sport, dip, dport);
	fprintf(fout, "%lu sockcalls\n", sockcalls.size());
	for (int i = 0; i < sockcalls.size(); i++){
		derand_rec_sockcall &sc = sockcalls[i];
		if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
			fprintf(fout, "sendmsg: 0x%x %lu\n", sc.sendmsg.flags, sc.sendmsg.size);
		}else 
			fprintf(fout, "recvmsg: 0x%x %lu\n", sc.recvmsg.flags, sc.recvmsg.size);
	}
	fprintf(fout, "%lu events\n", evts.size());
	for (int i = 0; i < evts.size(); i++){
		derand_event &e = evts[i];
		fprintf(fout, "%u %u\n", e.seq, e.type);
	}
	fprintf(fout, "%lu jiffies_reads\n", jiffies_reads.size());
	int cnt[256] = {0};
	for (int i = 0; i < jiffies_reads.size(); i++){
		cnt[jiffies_reads[i].id]++;
		//fprintf(fout, "%hhu\n", jiffies_reads[i].id);
	}
	for (int i = 0; i < 256; i++)
		if (cnt[i] > 0)
			fprintf(fout, "%d %d\n", i, cnt[i]);
}

void Records::clear(){
	evts.clear();
	sockcalls.clear();
}
