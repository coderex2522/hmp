#ifndef HMP_NODE_H
#define HMP_NODE_H

struct hmp_node{
	struct hmp_config config;
	struct hmp_context *ctx;

	struct hmp_transport listen_trans;
	
	int num_devices;
	struct list_head dev_list;
};

struct hmp_node curnode;

void hmp_node_init();

void hmp_node_release();
#endif