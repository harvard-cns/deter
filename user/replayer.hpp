#ifndef _USER__REPLAYER_HPP
#define _USER__REPLAYER_HPP

#include <string>
#include <thread>
#include "deter_replayer.hpp"
#include "records.hpp"

class Replayer{
public:
	DeterReplayer *d;
	Records rec;
	int sockfd; // the working socket

	Replayer();
	void set_addr(void *addr){d = (DeterReplayer*)addr;}
	int read_records(const std::string &record_file_name);
	int convert_event();
	#if USE_PKT_STREAM
	int convert_ps();
	#else
	int convert_drop();
	#endif
	int convert_jiffies();
	int convert_memory_pressure();
	int convert_memory_allocated();
	int convert_mstamp();
	int convert_siq();
	int convert_effect_bool();
	#if ADVANCED_EVENT_ENABLE
	int convert_advanced_event();
	#endif

	/* start the server side replay */
	int start_replay_server();
	int start_replay_client(const std::string &dip);
	void start_replay();
	void sockcall_thread(u64 id);
};

#endif /* _USER__REPLAYER_HPP */
