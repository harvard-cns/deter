#ifndef _RECORDS_HPP
#define _RECORDS_HPP

#include <vector>
#include <cstdio>
#include <string>
#include "derand.h"

class Records{
public:
	uint32_t recorder_id;
	uint32_t sip, dip;
	uint16_t sport, dport;
	std::vector<derand_event> evts;
	std::vector<derand_rec_sockcall> sockcalls;
	std::vector<jiffies_read> jiffies_reads;
	std::vector<uint32_t> memory_pressures;
	uint32_t n_memory_allocated;
	uint32_t n_sockets_allocated;
	uint32_t mstamp[16];
	uint32_t effect_bool[16];

	Records() : recorder_id(-1) {}
	int dump(const char* filename = NULL);
	int read(const char* filename);
	void print(FILE* fout = stdout);
	void clear();
};

#endif /* _RECORDS_HPP */
