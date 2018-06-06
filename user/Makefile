recorder : recorder.cpp mem_share.o records.o derand.h
	g++ recorder.cpp mem_share.o records.o -o recorder -O3 -std=gnu++11

mem_share.o : mem_share.cpp mem_share.hpp
	g++ mem_share.cpp -c -o mem_share.o -O3 -std=gnu++11

records.o: records.cpp records.hpp derand.h
	g++ records.cpp -c -o records.o -O3 -std=gnu++11

reader: reader.cpp records.o
	g++ reader.cpp records.o -o reader -O3 -std=gnu++11
