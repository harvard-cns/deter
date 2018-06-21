#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "replayer.hpp"

using namespace std;

#define COPY_ONCE 1

Replayer::Replayer(): d(NULL) {}

int Replayer::convert_event(){
	auto &s = rec.evts;
	u64 size = s.size();
	if (size > EVENT_Q_LEN){
		fprintf(stderr, "too many events: %lu > %u\n", size, EVENT_Q_LEN);
		return -1;
	}
	d->evtq.h = 0;
	d->evtq.t = size;
	memcpy(d->evtq.v, &s[0], sizeof(derand_event) * size);
	return 0;
}

int Replayer::convert_jiffies(){
	auto &s = rec.jiffies;
	u64 size = s.size();
	if (size > JIFFIES_Q_LEN){
		fprintf(stderr, "too many jiffies: %lu > %u\n", size, EVENT_Q_LEN);
		return -1;
	}
	d->jfq.h = 0;
	d->jfq.t = size;
	memcpy(d->jfq.v, &s[0], sizeof(jiffies_rec) * size);
	return 0;
}

int Replayer::convert_memory_pressure(){
	auto &s = rec.mpq.v;
	u64 size = s.size();
	if (size > MEMORY_PRESSURE_Q_LEN){
		fprintf(stderr, "too many memory pressures: %lu > %u\n", size, MEMORY_PRESSURE_Q_LEN);
		return -1;
	}
	d->mpq.h = 0;
	d->mpq.t = size;
	memcpy(d->mpq.v, &s[0], sizeof(uint32_t) * size);

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
		auto &s = rec.ebq[i];
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

#if DERAND_DEBUG
int Replayer::convert_general_event(){
	auto &s = rec.geq;
	if (s.size() > GENERAL_EVENT_Q_LEN){
		fprintf(stderr, "too many general events: %lu > %u\n", s.size(), GENERAL_EVENT_Q_LEN);
		return -1;
	}
	d->geq.h = 0;
	d->geq.t = s.size();
	memcpy(d->geq.v, &s[0], sizeof(GeneralEvent) * s.size());
	return 0;
}
#endif /* DERAND_DEBUG */

int Replayer::read_records(const string &record_file_name){
	if (rec.read(record_file_name.c_str())){
		fprintf(stderr, "cannot read file: %s\n", record_file_name.c_str());
		return -1;
	}

	// make sure records is in final format
	rec.transform();

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
	#if DERAND_DEBUG
	if (convert_general_event())
		return -8;
	#endif
	return 0;
}

int Replayer::start_replay_server(){
	// first listen, and wait for a working socket
	int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock_fd < 0)
		return -1;
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(rec.sport);
	if (bind(listen_sock_fd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		return -2;
	listen(listen_sock_fd, 1);

	// get a working socket sockfd
	sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	sockfd = accept(listen_sock_fd, (sockaddr *) &client_addr, &client_addr_len);
	close(listen_sock_fd);

	// start replay
	start_replay();
}

void sockcall_thread(int sockfd, derand_rec_sockcall sc, int id){
	if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
		vector<uint8_t> buf(sc.sendmsg.size);
		send(sockfd, &buf[0], sc.sendmsg.size, sc.sendmsg.flags);
	} else if (sc.type == DERAND_SOCKCALL_TYPE_RECVMSG){
		vector<uint8_t> buf(sc.recvmsg.size);
		recv(sockfd, &buf[0], sc.recvmsg.size, sc.recvmsg.flags);
	}
	printf("sockcall %d finishes\n", id);
}

void Replayer::start_replay(){
	vector<thread> thread_pool;
	volatile uint32_t &seq = d->seq;
	volatile uint32_t &h = d->evtq.h;
	for (int i = 0; i < rec.sockcalls.size(); i++){
		// wait for this socket call to be issued
		if (h >= d->evtq.t)
			printf("Error: more sockcall than recorded\n");

		// wait until next event is sockcall lock, and the sockcall id matches
		while (!(seq == d->evtq.v[get_event_q_idx(h)].seq && d->evtq.v[get_event_q_idx(h)].type == i + DERAND_SOCK_ID_BASE));

		// issue the socket call
		thread_pool.push_back(thread(sockcall_thread, sockfd, rec.sockcalls[i], i));
	}

	// wait for all sockcall to finish
	for (int i = 0; i < thread_pool.size(); i++)
		thread_pool[i].join();
	printf("wait to finish\n");
	while (h < d->evtq.t - 1);
	printf("finish!\n");
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
