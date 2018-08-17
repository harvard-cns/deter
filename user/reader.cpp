#include "records.hpp"

int main(int argc, char **argv){
	if (argc != 2){
		fprintf(stderr, "usage: ./reader <record_file>\n");
		return -1;
	}
	Records rec;
	rec.read(argv[1]);
	#if 0
	rec.print_raw_storage_size();
	printf("\n");
	#endif
	#if 1
	rec.print_compressed_storage_size();
	printf("\n");
	#endif
	rec.print_init_data();
	rec.print();
	return 0;
}
