#ifndef _USER__REPLAYER_HPP
#define _USER__REPLAYER_HPP

#include <string>
#include <thread>
#include "derand_replayer.hpp"
#include "records.hpp"

class Replayer{
public:
	derand_replayer *d;
	Records rec;
	int sockfd; // the working socket

	Replayer();
	void set_addr(void *addr){d = (derand_replayer*)addr;}
	int read_records(const std::string &record_file_name);
	int convert_event();
	int convert_drop();
	int convert_jiffies();
	int convert_memory_pressure();
	int convert_memory_allocated();
	int convert_mstamp();
	int convert_effect_bool();
	#if DERAND_DEBUG
	int convert_general_event();
	#endif

	/* start the server side replay */
	int start_replay_server();
	void start_replay();
	void sockcall_thread(u64 id);
};

#endif /* _USER__REPLAYER_HPP */
