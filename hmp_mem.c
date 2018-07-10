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
	mempool->alloc_size=0;
	mempool->mr=ibv_reg_mr(curnode.listen_trans->device->pd, mempool->base_addr, 
							mempool->size,
							IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
							IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
	
	if(!mempool->mr){
		ERROR_LOG("rdma register memory error.");
		goto cleanpagearray;
	}
	
	return mempool;

cleanpagearray:
	free(mempool->page_array);
	
cleanbaseaddr:
	free(mempool->base_addr);
	
cleanmempool:
	free(mempool);
	
	return NULL;
}

void *hmp_get_hybrid_mempool_addr()
{
	long long tmp=curnode.config.curnode_id;
	tmp=tmp<<48;
	return (void*)tmp;
}

void hmp_mem_init()
{
	int curnode_id;
	
	/*set the start addr of the hybrid mempool*/
	curnode.hybrid_mempool_base=hmp_get_hybrid_mempool_addr();
	INFO_LOG("curnode hybrid mem addr %p",curnode.hybrid_mempool_base);
	
	curnode.dram_mempool=hmp_mempool_create(HMP_DRAM_MEM_SIZE);
	if(!curnode.dram_mempool){
		ERROR_LOG("allocate dram_mempool error.");
		exit(-1);
	}
	INFO_LOG("create dram_mempool success. %ld %p",curnode.dram_mempool->mr->rkey,curnode.dram_mempool->base_addr);
	
	
	curnode.nvm_mempool=hmp_mempool_create(HMP_NVM_MEM_SIZE);
	if(!curnode.nvm_mempool){
		ERROR_LOG("allocate nvm_mempool error.");
		exit(-1);
	}
	INFO_LOG("create nvm_mempool success.%ld %p",curnode.nvm_mempool->mr->rkey,curnode.nvm_mempool->base_addr);

	/*set config curnode rkey*/
	curnode_id=curnode.config.curnode_id;
	curnode.config.node_infos[curnode_id].dram_base_addr=curnode.dram_mempool->base_addr;
	curnode.config.node_infos[curnode_id].dram_rkey=curnode.dram_mempool->mr->rkey;
	curnode.config.node_infos[curnode_id].nvm_base_addr=curnode.nvm_mempool->base_addr;
	curnode.config.node_infos[curnode_id].nvm_rkey=curnode.nvm_mempool->mr->rkey;
	
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

void* hmp_malloc(int length)
{
	//need pthread_mutex protected;
	void *res=NULL;
	assert(length>0);
	res=curnode.dram_mempool->base_addr+curnode.dram_mempool->alloc_size;
	curnode.dram_mempool->alloc_size+=length;
	return res;
}



void hmp_free(void *addr, int length)
{
	
}

int hmp_get_hm_node_id(void **hm_addr)
{
	long long tmp=(long long)*hm_addr;
	int node_id;
	node_id=tmp>>48;
	tmp=tmp%281474976710656;
	*hm_addr=(void*)tmp;
	INFO_LOG("hm_src %d",node_id);
	INFO_LOG("hm_addr %p\n",*hm_addr);
	return node_id;
}


int hmp_read(void *local_dst, void *hm_src, int length)
{
	int hm_node_id=0;

	hm_node_id=hmp_get_hm_node_id(&hm_src);
	if(hm_node_id==curnode.config.curnode_id)
		memcpy(local_dst,hm_src,length);
	else{
		;
	}	
	return 0;	
}

int hmp_write(void *hm_dst, void *local_src, int length)
{
	int hm_node_id=0;
	hm_node_id=hmp_get_hm_node_id(&hm_dst);
	if(hm_node_id==curnode.config.curnode_id)
		memcpy(hm_dst,local_src,length);
	else{
		;
	}
	return 0;
}




