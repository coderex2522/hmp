#ifndef HMP_CONFIG_H
#define HMP_CONFIG_H

#define HMP_CONFIG_FILE "config.xml"
#define HMP_ADDR_LEN 18
#define HMP_NODE_NUM 4

struct hmp_node_info{
	char addr[HMP_ADDR_LEN];
	int id;
	int port;
};

struct hmp_config{
	struct hmp_node_info node_infos[HMP_NODE_NUM];
	int node_cnt;
};

struct hmp_config global_config;
#endif