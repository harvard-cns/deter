#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "replayer.hpp"

using namespace std;

#define CONTINUOUS_COPY 1

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
	if (size > 0)
		memcpy(d->evtq.v, &s[0], sizeof(derand_event) * size);
	return 0;
}

#if USE_PKT_STREAM
int Replayer::convert_ps(){
	auto &s = rec.ps;
	u64 size = s.size();
	if (size > PS_Q_LEN){
		fprintf(stderr, "too many ps: %lu > %u\n", size, PS_Q_LEN);
		return -1;
	}
	d->ps.h = 0;
	d->ps.t = size;
	if (size > 0)
		memcpy(d->ps.v, &s[0], sizeof(uint32_t) * size);
	return 0;
}
#else
int Replayer::convert_drop(){
	auto &s = rec.dpq;
	u64 size = s.size();
	if (size > DROP_Q_LEN){
		fprintf(stderr, "too many drops: %lu > %u\n", size, DROP_Q_LEN);
		return -1;
	}
	d->dpq.h = 0;
	d->dpq.t = size;
	if (size > 0)
		memcpy(d->dpq.v, &s[0], sizeof(uint32_t) * size);
	return 0;
}
#endif

int Replayer::convert_jiffies(){
	auto &s = rec.jiffies;
	u64 size = s.size();
	if (size > JIFFIES_Q_LEN){
		fprintf(stderr, "too many jiffies: %lu > %u\n", size, EVENT_Q_LEN);
		return -1;
	}
	d->jfq.h = 0;
	d->jfq.t = size;
	if (size > 0)
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
	if (size > 0)
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
	if (s.size() > 0)
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
	if (s.size() > 0)
		memcpy(d->msq.v, &s[0], sizeof(skb_mstamp) * s.size());
	return 0;
}

int Replayer::convert_siq(){
	auto &s = rec.siq;
	if (s.n > SKB_IN_QUEUE_Q_LEN){
		fprintf(stderr, "to many siqq: %u > %u\n", s.n, SKB_IN_QUEUE_Q_LEN);
		return -1;
	}
	d->siqq.h = 0;
	d->siqq.t = s.n;
	if (s.v.size() > 0)
		memcpy(d->siqq.v, &s.v[0], sizeof(uint32_t) * s.v.size());
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
		if (s.v.size() > 0)
			memcpy(d->ebq[i].v, &s.v[0], sizeof(uint32_t) * s.v.size());
	}
	return 0;
}

#if ADVANCED_EVENT_ENABLE
int Replayer::convert_advanced_event(){
	auto &s = rec.aeq;
	if (s.size() > ADVANCED_EVENT_Q_LEN){
		fprintf(stderr, "too many advanced events: %lu > %u\n", s.size(), ADVANCED_EVENT_Q_LEN);
		return -1;
	}
	d->aeq.h = d->aeq.i = 0;
	d->aeq.t = s.size();
	if (s.size() > 0)
		memcpy(d->aeq.v, &s[0], sizeof(s[0]) * s.size());
	return 0;
}
#endif

int Replayer::read_records(const string &record_file_name){
	if (rec.read(record_file_name.c_str())){
		fprintf(stderr, "cannot read file: %s\n", record_file_name.c_str());
		return -1;
	}

	// make sure records is in final format
	rec.transform();

	// make d out of rec
	d->mode = rec.mode;
	if (rec.mode == 0) // server mode
		d->port = htons(rec.sport);
	else
		d->port = htons(rec.dport);
	d->init_data = rec.init_data;
	d->seq = 0;
	d->sockcall_id.counter = 0;
	
	if (convert_event())
		return -2;
	#if USE_PKT_STREAM
	if (convert_ps())
		return -3;
	#else
	if (convert_drop())
		return -3;
	#endif
	if (convert_jiffies())
		return -4;
	if (convert_memory_pressure())
		return -5;
	if (convert_memory_allocated())
		return -6;
	if (convert_mstamp())
		return -7;
	if (convert_siq())
		return -8;
	if (convert_effect_bool())
		return -9;
	#if ADVANCED_EVENT_ENABLE
	if (convert_advanced_event())
		return -10;
	#endif
	return 0;
}

int Replayer::start_replay_server(){
	// first listen, and wait for a working socket
	int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock_fd < 0){
		perror("socket");
		return -1;
	}
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(rec.sport);
	if (bind(listen_sock_fd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0){
		perror("bind");
		return -2;
	}
	listen(listen_sock_fd, 1);
	printf("listen sock created, listen on %hu\n", server_addr.sin_port);

	// get a working socket sockfd
	sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	sockfd = accept(listen_sock_fd, (sockaddr *) &client_addr, &client_addr_len);
	close(listen_sock_fd);
	printf("client sock created\n");

	// start replay
	start_replay();

	return 0;
}

int Replayer::start_replay_client(const string &dip){
	sockaddr_in server_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		perror("socket");
		return -1;
	}
	server_addr.sin_family = AF_INET;
	inet_aton(dip.c_str(), &server_addr.sin_addr);
	server_addr.sin_port = htons(rec.dport);
	if (connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
		perror("connect");
		return -2;
	}

	start_replay();

	return 0;
}

void do_sockcall(int sockfd, derand_rec_sockcall sc, int id){
	if (sc.type == DERAND_SOCKCALL_TYPE_SENDMSG){
		vector<uint8_t> buf(sc.sendmsg.size);
		send(sockfd, &buf[0], sc.sendmsg.size, sc.sendmsg.flags);
	} else if (sc.type == DERAND_SOCKCALL_TYPE_RECVMSG){
		vector<uint8_t> buf(sc.recvmsg.size);
		recv(sockfd, &buf[0], sc.recvmsg.size, sc.recvmsg.flags);
	} else if (sc.type == DERAND_SOCKCALL_TYPE_CLOSE){
		close(sockfd);
	} else if (sc.type == DERAND_SOCKCALL_TYPE_SETSOCKOPT){
		setsockopt(sockfd, sc.setsockopt.level, sc.setsockopt.optname, sc.setsockopt.optval, sc.setsockopt.optlen);
	}
	printf("sockcall %d finishes\n", id);
}

void Replayer::sockcall_thread(u64 id){
	volatile uint32_t &seq = d->seq;
	volatile uint32_t &h = d->evtq.h;
	for (int i = 0; i < rec.sockcalls.size(); i++){
		if (rec.sockcalls[i].thread_id != id)
			continue;
		if (h >= d->evtq.t)
			printf("Error: more sockcall than recorded\n");

		// wait until next event this sockcall lock
		while (!(seq == d->evtq.v[get_event_q_idx(h)].seq && get_sockcall_idx(d->evtq.v[get_event_q_idx(h)].type) == i));

		// do the sockcall
		do_sockcall(sockfd, rec.sockcalls[i], i);
	}
}

volatile int finished = 0;
void monitor_thread(DeterReplayer *d){
	while (!finished){
		printf("%u %u\n", d->seq, d->evtq.v[get_event_q_idx(d->evtq.h)].seq);
		sleep(1);
	}
}

void Replayer::start_replay(){
	thread monitor(monitor_thread, d);
	vector<thread> thread_pool;
	volatile uint32_t &seq = d->seq;
	volatile uint32_t &h = d->evtq.h;
	// create required number of threads
	int n_thread = 0;
	for (int i = 0; i < rec.sockcalls.size(); i++)
		if (rec.sockcalls[i].thread_id > n_thread)
			n_thread = rec.sockcalls[i].thread_id;
	n_thread++; // thread_id are numberred from 0
	for (int i = 0; i < n_thread; i++)
		thread_pool.push_back(thread(&Replayer::sockcall_thread, this, (u64)i));

	#if 0
	for (int i = 0; i < rec.sockcalls.size(); i++){
		// wait for this socket call to be issued
		if (h >= d->evtq.t)
			printf("Error: more sockcall than recorded\n");

		// wait until next event is sockcall lock, and the sockcall id matches
		while (!(seq == d->evtq.v[get_event_q_idx(h)].seq && d->evtq.v[get_event_q_idx(h)].type == i + DERAND_SOCK_ID_BASE));

		// issue the socket call
		thread_pool.push_back(thread(sockcall_thread, sockfd, rec.sockcalls[i], i));
	}
	#endif

	// wait for all sockcall to finish
	for (int i = 0; i < thread_pool.size(); i++)
		thread_pool[i].join();
	finished = 1;
	printf("wait to finish\n");
	while (seq < d->evtq.v[get_event_q_idx(d->evtq.t)].seq);
	printf("finish!\n");
	//close(sockfd);
}
