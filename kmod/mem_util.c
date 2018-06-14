#include "mem_util.h"

int get_page_order(int n){
	int order = 0;
	while ((1<<order) * 4096 < n)
		order++;
	return order;
}
void reserve_pages(struct page* pg, int n_page){
	struct page *end = pg + n_page;
	for (; pg < end; pg++)
		SetPageReserved(pg);
}
void unreserve_pages(struct page* pg, int n_page){
	struct page *end = pg + n_page;
	for (; pg < end; pg++)
		ClearPageReserved(pg);
}
