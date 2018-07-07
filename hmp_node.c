#include "hmp_base.h"
#include "hmp_murmur_hash.h"
#include "hmp_log.h"
#include "hmp_config.h"
#include "hmp_context.h"
#include "hmp_transport.h"
#include "hmp_node.h"
#include "hmp_mem.h"

struct hmp_node curnode;

static void hmp_hash_init()
{
	struct hmp_hash_node *hash_node, *tmp;
	char key[50],port_str[10];
	int i,addr_len,flag;
	
	INIT_LIST_HEAD(&curnode.hash_node_list);

	for(i=0;i<curnode.config.node_cnt;i++){
		snprintf(port_str,sizeof(port_str),"%d",curnode.config.node_infos[i].port);
		
		addr_len=strlen(curnode.config.node_infos[i].addr);
		memcpy(key, curnode.config.node_infos[i].addr, addr_len);
		memcpy(key+addr_len, port_str, strlen(port_str)+1);
		
		hash_node=(struct hmp_hash_node*)malloc(sizeof(struct hmp_hash_node));
		if(!hash_node){
			ERROR_LOG("allocate memory error.");
			exit(-1);
		}
		hash_node->key=murmur_hash(key, strlen(key)+1);
		hash_node->node_id=i;
		INIT_LIST_HEAD(&hash_node->hash_node_list_entry);
		
		if(list_empty(&curnode.hash_node_list))
			list_add(&hash_node->hash_node_list_entry, &curnode.hash_node_list);
		else{
			flag=1;
			list_for_each_entry(tmp, &curnode.hash_node_list, hash_node_list_entry){
				if(tmp->key>hash_node->key){
					flag=0;
					list_add_tail(&hash_node->hash_node_list_entry, 
								&tmp->hash_node_list_entry);
					break;
				}
			}
			if(flag)
				list_add_tail(&hash_node->hash_node_list_entry, &curnode.hash_node_list);
			
		}
	}

	list_for_each_entry(tmp, &curnode.hash_node_list, hash_node_list_entry){
		INFO_LOG("key %lu id %d",tmp->key,tmp->node_id);
	}
}

void hmp_node_init()
{
	int i;
	/*get the config.xml information*/
	hmp_config_init(&curnode.config);
	/*build the hash node which is used for consistent hashing*/
	hmp_hash_init();
	
	hmp_transport_init();

	curnode.ctx=hmp_context_create();
	if(!curnode.ctx){
		ERROR_LOG("create context error.");
		exit(-1);
	}
	
	curnode.listen_trans=hmp_transport_create(&curnode, curnode.ctx);
	if(!curnode.listen_trans){
		ERROR_LOG("create listen trans error.");
		exit(-1);
	}
	
	memset(curnode.connect_trans, 0, HMP_NODE_NUM*sizeof(struct hmp_transport*));
	hmp_transport_listen(curnode.listen_trans, 
					curnode.config.node_infos[curnode.config.curnode_id].port);

	for(i=curnode.config.curnode_id+1;i<curnode.config.node_cnt;i++){
		INFO_LOG("create the [%d]-th transport.",i);
		curnode.connect_trans[i]=hmp_transport_create(&curnode, curnode.ctx);
		if(!curnode.connect_trans[i]){
			ERROR_LOG("create rdma trans error.");
			exit(-1);
		}
		hmp_transport_connect(curnode.connect_trans[i],
							curnode.config.node_infos[i].addr,
							curnode.config.node_infos[i].port);
	}

	/*init mempool*/
	hmp_mem_init();
}


void hmp_node_release()
{
	//sleep(5);
	//curnode.ctx->stop=1;
	pthread_join(curnode.ctx->epoll_thread, NULL);
	INFO_LOG("hmp node release.");
	hmp_transport_release();
	hmp_mem_destroy();
}


