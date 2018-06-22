#ifndef HMP_NODE_H
#define HMP_NODE_H

struct hmp_hash_node{
	uint32_t key;
	int node_id;
	struct list_head hash_node_list_entry;
};

struct hmp_node{
	struct hmp_config config;
	struct hmp_context *ctx;

	int listen_port;
	struct hmp_transport *listen_trans;
	struct hmp_transport *connect_trans[HMP_NODE_NUM];

	/*about memory*/
	struct hmp_mempool *dram_mempool;
	void *hybrid_mempool_base;
	long long alloc_len;
	
	int num_devices;
	struct list_head dev_list;

	struct list_head hash_node_list;
};

void hmp_node_init();

void hmp_node_release();
#endif