#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_config.h"
#include "hmp_context.h"
#include "hmp_transport.h"
#include "hmp_node.h"
#include "hmp_mem.h"

extern struct hmp_node curnode;

static struct hmp_mempool *hmp_mempool_create(int size)
{
	struct hmp_mempool *mempool;

	mempool=(struct hmp_mempool*)calloc(1,sizeof(struct hmp_mempool));
	if(!mempool){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	mempool->page_num=size/HMP_PAGE_SIZE;
	if(size%HMP_PAGE_SIZE)
		mempool->page_num+=1;
	mempool->size=mempool->page_num*HMP_PAGE_SIZE;
	mempool->base_addr=calloc(1,mempool->size);
	if(!mempool->base_addr){
		ERROR_LOG("allocate memory error.");
		goto cleanmempool;
	}

	mempool->page_array=calloc(mempool->page_num,sizeof(struct hmp_page));
	if(!mempool->page_array){
		ERROR_LOG("allocate memory error.");
		goto cleanbaseaddr;
	}
	/*note:lack the page arr init*/
	return mempool;
cleanbaseaddr:
	free(mempool->base_addr);
	
cleanmempool:
	free(mempool);
	return NULL;
}

void hmp_mem_init()
{
	curnode.dram_mempool=hmp_mempool_create(1024*1024);
	if(!curnode.dram_mempool){
		ERROR_LOG("allocate mempool error.");
		exit(-1);
	}
	INFO_LOG("create mempool success.");

	/*set the start addr of the hybrid mempool*/
	curnode.hybrid_mempool_base=(void *)(HMP_DRAM_MEM_SIZE*curnode.config.curnode_id);
	INFO_LOG("base %p",curnode.hybrid_mempool_base);
	//alloc memory len
	curnode.alloc_len=0;
}

void hmp_mem_destroy()
{
	if(curnode.dram_mempool){
		free(curnode.dram_mempool->page_array);
		free(curnode.dram_mempool->base_addr);
		free(curnode.dram_mempool);
		curnode.dram_mempool=NULL;
	}
}

static int hmp_lenintopow2(int length)
{
    int tmplength,reallength;
    tmplength=length;
	reallength=1;
	while(length!=0){
		length=length>>1;
		reallength=reallength<<1;
	}
	if(tmplength*2==reallength)
        reallength=tmplength;
	return reallength;
}

void* hmp_malloc(int length)
{
	void *res=NULL;
	int rlength;
	if(length<=0){
		ERROR_LOG("wait for alloc memory length is %d error",length);
		return NULL;
	}

	rlength=hmp_lenintopow2(length);
	
	if(curnode.alloc_len+rlength<HMP_DRAM_MEM_SIZE){
		res=curnode.hybrid_mempool_base+curnode.alloc_len;
		curnode.alloc_len+=rlength;
		return res;
	}
	else{
		ERROR_LOG("alloc memory error.");
		return NULL;
	}
}


/*
void hmp_free(void *addr, int length)
{
	
}

int hmp_read(void *local_dst, void *hm_src, int length)
{

}

int hmp_write(void *hm_dst, void *local_src, int length)
{
	
}
*/



