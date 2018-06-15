#ifndef _USER__REPLAYER_HPP
#define _USER__REPLAYER_HPP

#include <string>
#include "derand_replayer.hpp"
#include "records.hpp"

class Replayer{
public:
	derand_replayer *d;
	Records rec;

	Replayer();
	void set_addr(void *addr){d = (derand_replayer*)addr;}
	int read_records(const std::string &record_file_name);
	int convert_event();
	int convert_jiffies();
	int convert_memory_pressure();
	int convert_memory_allocated();
	int convert_mstamp();
	int convert_effect_bool();
};

#endif /* _USER__REPLAYER_HPP */
