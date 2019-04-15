#ifndef _RECORDS_HPP
#define _RECORDS_HPP

#include <vector>
#include <cstdio>
#include <string>
#include <cassert>
#include <map>
#include "base_struct.hpp"
#include "coding.hpp"

static uint32_t nbit_dynamic_coding(uint64_t x, uint64_t step = 0){
	// Dynamically increase the nbits for recording x
	// The assumption is that smaller x is much more frequent than larger x, especially x = 1
	// 1: 0
	// 2: 1,00
	// 3: 1,01
	// 4: 1,10
	// 5: 1,11,0000
	// 6: 1,11,0001
	// ...
	// 19: 1,11,1110
	// 20: 1,11,1111,00000000
	uint32_t w = 1, nbit = 0;
	if (step == 0){
		for (; x; w*=2){
			nbit += w;
			if (x <= (1lu<<w)-1)
				break;
			x -= (1lu<<w) - 1;
		}
	}else{
		for (w = (step & 0xff) + 1; x; w = (step & 0xff) + 1){
			nbit += w;
			if (x <= (1lu << w) - 1)
				break;
			x -= (1lu << w) - 1;
			if (step >> 8)
				step >>= 8;
		}
	}
	return nbit;
}

static inline int get_nbit(uint64_t x){
	return 64 - __builtin_clzl(x);
}
static uint32_t nbit_static_prefix_encoding(uint64_t x, int prefix_nbit){
	if (x==0)
		return prefix_nbit+1;
	int nbit = get_nbit(x);
	return nbit + prefix_nbit;
}

// format:
// | huffman coding of nbit | actual bits that represents the value |
// Use the first 'first_n' elements to build the huffman coding
// return total bits
static uint64_t nbit_huffman_prefix_encoding(std::vector<uint64_t> x, int first_n){
	uint64_t res = 0;
	std::vector<uint64_t> freq(65); // frequency of each nbit
	for (int i = 0; i < first_n && i < (int)x.size(); i++)
		freq[get_nbit(x[i])]++;
	auto coding = huffman(freq);
	#if 0
	for (int i = 0; i < 64; i++)
		printf("%d: %lu %u\n", i, freq[i], coding[i].nbit);
	#endif
	for (int i = 0; i < (int)x.size(); i++){
		int nbit = get_nbit(x[i]);
		res += nbit + coding[nbit].nbit;
		//printf("%lu: %d+%d %lu\n", x[i], coding[nbit].nbit, nbit, res);
	}
	for (int i = 0; i < (int)coding.size(); i++){
		if (freq[i] == 0)
			continue;
		res += 5 + coding[i].nbit; // 5 bit coding length, 32 bit coding
	}
	return res;
}

static uint64_t nbit_huffman_encoding(std::vector<uint64_t> x, int first_n){
	uint64_t res = 0;
	std::map<uint64_t, uint64_t> freq;
	for (int i = 0; i < first_n && i < (int)x.size(); i++)
		freq[x[i]]++;
	std::vector<uint64_t> a;
	for (auto &it : freq){
		a.push_back(it.second);
		it.second = a.size() - 1;
	}
	auto coding = huffman(a);
	#if 0
	for (auto it : freq){
		printf("%lu: %lu %u\n", it.first, a[it.second], coding[it.second].nbit);
	}
	#endif
	for (int i = 0; i < (int)x.size(); i++){
		int nbit = coding[freq[x[i]]].nbit;
		res += nbit;
		//printf("%lu: %d+%d\n", x[i], coding[nbit].nbit, nbit);
	}
	for (int i = 0; i < (int)coding.size(); i++)
		res += 5 + 32; // 5 bit coding length, 32 bit coding
	return res;
}

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
		return nbit_dynamic_coding(n)/8 + sizeof(uint32_t) * v.size();
	}
	uint64_t compressed_storage_size(){
		uint64_t res = raw_storage_size();
		uint64_t tmp = storage_size_index_x(0);
		if (tmp < res)
			res = tmp;
		tmp = storage_size_index_x(1);
		if (tmp < res)
			res = tmp;
		tmp = storage_size_delta_of_diff();
		if (tmp < res)
			res = tmp;
		return res;
	}
	uint64_t storage_size_index_x(int x){
		uint64_t res = 0;
		if (format == RAW){
			for (uint32_t i = 0, ib = 0; ib < n; i++)
				for (uint32_t j = 0; j < 32 && ib < n; j++, ib++)
					if (((v[i] >> j) & 1) == x)
						res += sizeof(uint32_t); // 4-byte index
			res += nbit_dynamic_coding(n) / 8;
		}else {
			res = -1;
		}
		return res;
	}
	uint64_t storage_size_delta_of_diff(){
		// In this way of storage, we record the number of consecutive bits that
		// are the same, called segments. The number of bits in each segment is 
		// its length.
		uint64_t res;

		// if no records, directly return
		if (v.size() == 0)
			return nbit_dynamic_coding(n) / 8;

		// records the length of each segment 
		std::vector<uint32_t> len;
		if (format == RAW){
			uint32_t cur_len = 0;
			uint8_t b = 0; // previous bit
			for (uint32_t i = 0, ib = 0; ib < n; i++)
				for (uint32_t j = 0; j < 32 && ib < n; j++, ib++)
					if (((v[i] >> j) & 1) == b)
						cur_len++;
					else {
						if (cur_len)
							len.push_back(cur_len);
						cur_len = 1;
						b ^= 1;
					}
			if (cur_len)
				len.push_back(cur_len);
			// Try different nbit to record segment length
			// If one segment is longer than the nbit can record,
			// use (2^nbit-1) to extend
			// For example, nbit = 4
			// 1: 0000
			// 2: 0001
			// ...
			// 15: 1110
			// 16: 1111,0000
			// ...
			// 30: 1111,1110
			// 31: 1111,1111,0000
			res = raw_storage_size();
			for (int nbit = 1; nbit < 32; nbit++){
				uint64_t tmp = 0, x = (1<<nbit)-1;
				for (uint32_t l : len){
					tmp += ((l-1) / x + 1) * nbit; // roundup(l/x) * nbit
				}
				tmp = (tmp + nbit_dynamic_coding(n)) / 8;
				if (tmp < res)
					res = tmp;
			}
			// Try dynamically increase the nbits for each segment
			// 1: 0
			// 2: 1,00
			// 3: 1,01
			// 4: 1,10
			// 5: 1,11,0000
			// 6: 1,11,0001
			// ...
			// 19: 1,11,1110
			// 20: 1,11,1111,00000000
			uint64_t tmp = 0;
			for (uint32_t l : len){
				tmp += nbit_dynamic_coding(l);
			}
			tmp = (tmp + nbit_dynamic_coding(n)) / 8;
			if (tmp < res)
				res = tmp;
			return res;
		}else {
			return -1;
		}
	}
};

class Records{
public:
	uint32_t mode, broken, alert;
	uint32_t recorder_id;
	uint32_t active;
	uint32_t sip, dip;
	uint16_t sport, dport;
	uint32_t fin_seq;
	tcp_sock_init_data init_data;
	std::vector<derand_event> evts;
	std::vector<derand_rec_sockcall> sockcalls;
	std::vector<uint32_t> dpq;
	#if GET_REORDER
	std::vector<uint8_t> reorder;
	#endif
	std::vector<jiffies_rec> jiffies;
	BitArray mpq;
	std::vector<memory_allocated_rec> memory_allocated;
	uint32_t n_sockets_allocated;
	std::vector<skb_mstamp> mstamp;
	std::vector<uint8_t> siqq;
	BitArray siq;
	BitArray ebq[DERAND_EFFECT_BOOL_N_LOC];
	#if COLLECT_TX_STAMP
	std::vector<uint32_t> tsq;
	#endif
	#if ADVANCED_EVENT_ENABLE
	std::vector<u32> aeq;
	#endif

	Records() : broken(0), alert(0), recorder_id(-1), active(0), fin_seq(0) {}
	void transform(); // transform raw data to final format
	void order_sockcalls(); // order sockcalls according to their first appearance in evts
	int dump(const char* filename = NULL);
	int read(const char* filename);
	void print_meta(FILE *fout = stdout);
	void print(FILE* fout = stdout);
	void print_init_data(FILE* fout = stdout);
	void clear();
	uint64_t get_pkt_received();
	uint64_t get_total_bytes_received();
	uint64_t get_total_bytes_sent();
	uint64_t sample_timestamp(std::vector<uint64_t> &v, uint64_t th);
	#if COLLECT_TX_STAMP
	uint64_t tx_stamp_size();
	#endif
	void print_raw_storage_size();
	derand_rec_sockcall* evt_get_sc(derand_event *evt);
	uint64_t compressed_evt_size();
	uint64_t compressed_sockcall_size();
	uint64_t compressed_memory_allocated_size();
	uint64_t compressed_mstamp_size();
	void print_compressed_storage_size();
};

#endif /* _RECORDS_HPP */
