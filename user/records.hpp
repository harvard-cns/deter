#ifndef _RECORDS_HPP
#define _RECORDS_HPP

#include <vector>
#include <cstdio>
#include <string>
#include <cassert>
#include "base_struct.hpp"

struct BitArray{
	u32 n;
	enum Format{
		RAW = 0,
		INDEX_ONE = 1, // record indexes for 1
		INDEX_ZERO = 2, // record indexes for 0
	} format;
	std::vector<uint32_t> v;

	BitArray() : format(RAW) {}
	void push_back(uint32_t x){
		v.push_back(x);
	}
	void clear(){
		n = 0;
		format = RAW;
		v.clear();
	}
	void __transform_to_idx_x(int x){
		std::vector<uint32_t> v2;
		for (uint32_t i = 0, ib = 0; ib < n; i++)
			for (uint32_t j = 0; j < 32 && ib < n; j++, ib++)
				if (((v[i] >> j) & 1) == x)
					v2.push_back(ib);
		v.clear();
		v = v2;
	}
	void transform_to_idx_one(){
		if (format == INDEX_ONE)
			return;
		assert(format == RAW);
		__transform_to_idx_x(1);
		format = INDEX_ONE;
	}
	void transform_to_idx_zero(){
		if (format == INDEX_ZERO)
			return;
		assert(format == RAW);
		__transform_to_idx_x(0);
		format = INDEX_ZERO;
	}
	int dump(FILE *fout){
		uint32_t len;
		if (!fwrite(&n, sizeof(n), 1, fout))
			return 0;
		if (!fwrite(&format, sizeof(format), 1, fout))
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
		if (!fread(&format, sizeof(format), 1, fin))
			return 0;
		if (!fread(&len, sizeof(len), 1, fin))
			return 0;
		v.resize(len);
		if (len > 0)
			if (!fread(&v[0], sizeof(uint32_t) * len, 1, fin))
				return 0;
		return 1;
	}
	void print(FILE* fout){
		if (format == RAW){
			fprintf(fout, "%u reads\n", n);
			for (uint32_t i = 0, ib = 0; ib < n; i++){
				for (uint32_t j = 0; j < 32 && ib < n; j++, ib++)
					fprintf(fout, "%u", (v[i] >> j) & 1);
				fprintf(fout, "\n");
			}
		}else if (format == INDEX_ONE || format == INDEX_ZERO){
			fprintf(fout, "%u reads, %lu '%d' indexes:\n", n, v.size(), format == INDEX_ONE);
			for (uint32_t i = 0; i < v.size(); i++)
				fprintf(fout, "%u\n", v[i]);
		}
	}

	uint64_t raw_storage_size(){
		return sizeof(n) + sizeof(uint32_t) * v.size();
	}
};

class Records{
public:
	uint32_t mode, broken;
	uint32_t recorder_id;
	uint32_t sip, dip;
	uint16_t sport, dport;
	tcp_sock_init_data init_data;
	std::vector<derand_event> evts;
	std::vector<derand_rec_sockcall> sockcalls;
	std::vector<uint32_t> dpq;
	std::vector<jiffies_rec> jiffies;
	BitArray mpq;
	std::vector<memory_allocated_rec> memory_allocated;
	uint32_t n_sockets_allocated;
	std::vector<skb_mstamp> mstamp;
	BitArray ebq[DERAND_EFFECT_BOOL_N_LOC];
	#if DERAND_DEBUG
	std::vector<GeneralEvent> geq;
	#endif

	Records() : recorder_id(-1) {}
	void transform(); // transform raw data to final format
	int dump(const char* filename = NULL);
	int read(const char* filename);
	void print(FILE* fout = stdout);
	void print_init_data(FILE* fout = stdout);
	void clear();
	void print_raw_storage_size();
	void print_compressed_storage_size();
};

#endif /* _RECORDS_HPP */
