#ifndef HMP_CONFIG_H
#define HMP_CONFIG_H

#define HMP_CONFIG_FILE "config.xml"
#define HMP_ADDR_LEN 18

struct hmp_node_info{
	char addr[HMP_ADDR_LEN];
	int port;
	void *dram_base_addr;
	uint32_t dram_rkey;
	void *nvm_base_addr;
	uint32_t nvm_rkey;
};

struct hmp_config{
	struct hmp_node_info node_infos[HMP_NODE_NUM];
	int curnode_id;
	int node_cnt;
};

int hmp_config_init(struct hmp_config *config);
#endif