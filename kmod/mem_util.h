#ifndef _MEM_UTIL_H
#define _MEM_UTIL_H

#include <linux/slab.h>
#include <linux/mm.h>

int get_page_order(int n);
void reserve_pages(struct page* pg, int n_page);
void unreserve_pages(struct page* pg, int n_page);

#endif /* _MEM_UTIL_H */
