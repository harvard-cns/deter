#ifndef _USER__REPLAYER_HPP
#define _USER__REPLAYER_HPP

#include <string>
#include "derand_replayer.hpp"
#include "records.hpp"

class Replayer{
public:
	derand_replayer d; // data
	void read_records(const std::string &record_file_name);
};

#endif /* _USER__REPLAYER_HPP */
