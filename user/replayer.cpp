#include <cstdio>
#include <cstring>
#include "replayer.hpp"

using namespace std;

Replayer::Replayer(): d(NULL) {}

int Replayer::convert_event(){
	auto &s = rec.evts;
	if (s.size() > EVENT_Q_LEN){
		fprintf(stderr, "too many events: %lu > %u\n", s.size(), EVENT_Q_LEN);
		return -1;
	}
	d->evtq.h = 0;
	d->evtq.t = s.size();
	memcpy(d->evtq.v, &s[0], sizeof(derand_event) * s.size());
	return 0;
}

int Replayer::convert_jiffies(){
	auto &s = rec.jiffies;
	if (s.size() > JIFFIES_Q_LEN){
		fprintf(stderr, "too many jiffies: %lu > %u\n", s.size(), EVENT_Q_LEN);
		return -1;
	}
	d->jfq.h = 0;
	d->jfq.t = s.size();
	memcpy(d->jfq.v, &s[0], sizeof(jiffies_rec) * s.size());
	return 0;
}

int Replayer::convert_memory_pressure(){
	d->mpq.h = d->mpq.t = 0;
	for (uint32_t i = 0, ib = 0; ib < rec.memory_pressures.n; i++)
		for (uint32_t j = 0; j < 32 && ib < rec.memory_pressures.n; j++, ib++)
			if ((rec.memory_pressures.v[i] >> j) & 1)
				if (d->mpq.t == MEMORY_PRESSURE_Q_LEN){
					fprintf(stderr, "too many memory_pressure==1\n");
					return -1;
				}
				else 
					d->mpq.v[d->mpq.t++] = ib;
	return 0;
}

int Replayer::convert_memory_allocated(){
	auto &s = rec.memory_allocated;
	if (s.size() > MEMORY_ALLOCATED_Q_LEN){
		fprintf(stderr, "too many memory_allocated: %lu > %u\n", s.size(), MEMORY_ALLOCATED_Q_LEN);
		return -1;
	}
	d->maq.h = 0;
	d->maq.t = s.size();
	memcpy(d->maq.v, &s[0], sizeof(memory_allocated_rec) * s.size());
	return 0;
}

int Replayer::convert_mstamp(){
	auto &s = rec.mstamp;
	if (s.size() > MSTAMP_Q_LEN){
		fprintf(stderr, "too many mstamp: %lu > %u\n", s.size(), MSTAMP_Q_LEN);
		return -1;
	}
	d->msq.h = 0;
	d->msq.t = s.size();
	memcpy(d->msq.v, &s[0], sizeof(skb_mstamp) * s.size());
	return 0;
}

int Replayer::convert_effect_bool(){
	for (int i = 0; i < DERAND_EFFECT_BOOL_N_LOC; i++){
		auto &s = rec.effect_bool[i];
		if (s.n > EFFECT_BOOL_Q_LEN){
			fprintf(stderr, "too many effect_bool %d: %u > %u\n", i, s.n, EFFECT_BOOL_Q_LEN);
			return -1;
		}
		d->ebq[i].h = 0;
		d->ebq[i].t = s.n;
		memcpy(d->ebq[i].v, &s.v[0], sizeof(uint32_t) * s.v.size());
	}
	return 0;
}

int Replayer::read_records(const string &record_file_name){
	if (rec.read(record_file_name.c_str())){
		fprintf(stderr, "cannot read file: %s\n", record_file_name.c_str());
		return -1;
	}

	// make d out of rec
	d->init_data = rec.init_data;
	d->seq = 0;
	d->sockcall_id.counter = 0;
	
	if (convert_event())
		return -2;
	if (convert_jiffies())
		return -3;
	if (convert_memory_pressure())
		return -4;
	if (convert_memory_allocated())
		return -5;
	if (convert_mstamp())
		return -6;
	if (convert_effect_bool())
		return -7;
	return 0;
}

/*
struct derand_replayer{
	struct tcp_sock_init_data init_data; // initial values for tcp_sock
	u32 seq;
	atomic_t sockcall_id; // current socket call ID
	struct event_q evtq;
	struct jiffies_q jfq;
	struct memory_pressure_q mpq;
	struct memory_allocated_q maq;
	struct mstamp_q msq;
	struct effect_bool_q ebq[DERAND_EFFECT_BOOL_N_LOC]; // effect_bool
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
*/
