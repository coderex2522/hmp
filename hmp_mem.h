#ifndef HMP_MEM_H
#define HMP_MEM_H

#define HMP_PAGE_SIZE 64
#define HMP_THREADSOLD_MEM (2*1024*1024)
#define HMP_DRAM_MEM_SIZE (4*1024*1024)

struct hmp_page{
	int count;
	struct list_head free_list_entry;
};

struct hmp_mempool{
	void *base_addr;
	struct hmp_page *page_array;
	int page_num;
	int size;//HMP_PAGE_SIZE*page_num
	
};

void hmp_mem_init();

void hmp_mem_destroy();
#endif