#ifndef _RECORDS_HPP
#define _RECORDS_HPP

#include <vector>
#include <cstdio>
#include <string>
#include "base_struct.hpp"

struct BitArray{
	u32 n;
	std::vector<uint32_t> v;
	void push_back(uint32_t x){
		v.push_back(x);
	}
	void clear(){
		n = 0;
		v.clear();
	}
	int dump(FILE *fout){
		uint32_t len;
		if (!fwrite(&n, sizeof(n), 1, fout))
			return 0;
		len = v.size();
		if (!fwrite(&len, sizeof(len), 1, fout))
			return 0;
		if (len > 0)
			if (!fwrite(&v[0], sizeof(uint32_t) * len, 1, fout))
				return 0;
		return 1;
	}
	int read(FILE *fin){
		uint32_t len;
		if (!fread(&n, sizeof(n), 1, fin))
			return 0;
		if (!fread(&len, sizeof(len), 1, fin))
			return 0;
		v.resize(len);
		if (len > 0)
			if (!fread(&v[0], sizeof(uint32_t) * len, 1, fin))
				return 0;
		return 1;
	}
};

class Records{
public:
	uint32_t recorder_id;
	uint32_t sip, dip;
	uint16_t sport, dport;
	tcp_sock_init_data init_data;
	std::vector<derand_event> evts;
	std::vector<derand_rec_sockcall> sockcalls;
	std::vector<jiffies_rec> jiffies;
	BitArray memory_pressures;
	std::vector<memory_allocated_rec> memory_allocated;
	uint32_t n_sockets_allocated;
	std::vector<skb_mstamp> mstamp;
	BitArray effect_bool[DERAND_EFFECT_BOOL_N_LOC];

	Records() : recorder_id(-1) {}
	int dump(const char* filename = NULL);
	int read(const char* filename);
	void print(FILE* fout = stdout);
	void print_init_data(FILE* fout = stdout);
	void clear();
};

#endif /* _RECORDS_HPP */
