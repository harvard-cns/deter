#include <string>
#include "records.hpp"

using namespace std;

void print_usage(){
	fprintf(stderr, "usage: ./reader <record_file> [<get_meta>]\n");
}

int main(int argc, char **argv){
	if (argc != 2 && argc != 3){
		print_usage();
		return -1;
	}
	Records rec;
	rec.read(argv[1]);
	string get_meta = "";
	if (argc == 3){
		get_meta = argv[2];
		if (get_meta != "get_meta"){
			print_usage();
			return -1;
		}
		printf("=====storage_size=====\n");
		rec.print_compressed_storage_size();
		printf("\n");
		printf("=====metadata=====\n");
		rec.print_meta();
		printf("\n");
		return 0;
	}
	#if 0
	rec.print_raw_storage_size();
	printf("\n");
	#endif
	#if 1
	printf("=====storage_size=====\n");
	rec.print_compressed_storage_size();
	printf("\n");
	#endif
	printf("=====metadata=====\n");
	rec.print_meta();
	printf("\n");
	printf("=====init_data=====\n");
	rec.print_init_data();
	printf("\n");
	printf("=======data=======\n");
	rec.print();
	return 0;
}
