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
	tcp_sock_init_data init_data;
	std::vector<derand_event> evts;
	std::vector<derand_rec_sockcall> sockcalls;
	std::vector<jiffies_rec> jiffies;
	std::vector<uint32_t> memory_pressures;
	std::vector<memory_allocated_rec> memory_allocated;
	uint32_t n_sockets_allocated;
	std::vector<skb_mstamp> mstamp;
	uint32_t effect_bool[16];

	Records() : recorder_id(-1) {}
	int dump(const char* filename = NULL);
	int read(const char* filename);
	void print(FILE* fout = stdout);
	void print_init_data(FILE* fout = stdout);
	void clear();
};

#endif /* _RECORDS_HPP */
