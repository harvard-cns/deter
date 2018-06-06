#ifndef _RECORDS_HPP
#define _RECORDS_HPP

#include <vector>
#include <cstdio>
#include <string>
#include "derand.h"

class Records{
public:
	uint32_t recorder_id;
	uint32_t sip, dip;
	uint16_t sport, dport;
	std::vector<derand_event> evts;
	std::vector<derand_rec_sockcall> sockcalls;

	Records() : recorder_id(-1) {}
	int dump(const char* filename = NULL);
	void clear();
};

#endif /* _RECORDS_HPP */
