#include "records.hpp"

int main(int argc, char **argv){
	if (argc != 2){
		fprintf(stderr, "usage: ./reader <record_file>\n");
		return -1;
	}
	Records rec;
	rec.read(argv[1]);
	rec.print();
	return 0;
}
