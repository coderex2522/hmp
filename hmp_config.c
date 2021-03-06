#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_config.h"


#define HMP_NODES_INFO_STR "nodes_info"
#define HMP_NODE_STR "node"
#define HMP_ID_STR "id"
#define HMP_ADDR_STR "addr"
#define HMP_PORT_STR "port"
#define HMP_IB_DEVICE "ib0"//siw_ens33

static void hmr_print_nodes_info(struct hmp_config *global_config)
{
	int i;
	for(i=0;i<global_config->node_cnt;i++){
		INFO_LOG("addr %s",global_config->node_infos[i].addr);
		INFO_LOG("port %d",global_config->node_infos[i].port);
	}
}

static int hmp_parse_node(struct hmp_config *config, int index, xmlDocPtr doc, xmlNodePtr curnode)
{
	xmlChar *val;

	curnode=curnode->xmlChildrenNode;
	while(curnode!=NULL){
		if(!xmlStrcmp(curnode->name, (const xmlChar *)HMP_ADDR_STR)){
			val=xmlNodeListGetString(doc, curnode->xmlChildrenNode, 1);
			memcpy(config->node_infos[index].addr,
				(const void*)val,strlen((const char*)val)+1);
			xmlFree(val);
		}

		if(!xmlStrcmp(curnode->name, (const xmlChar *)HMP_PORT_STR)){
			val=xmlNodeListGetString(doc, curnode->xmlChildrenNode, 1);
			config->node_infos[index].port=atoi((const char*)val);
			xmlFree(val);
		}

		curnode=curnode->next;
	}
	return 0;
}

static void hmp_set_curnode_id(struct hmp_config *config)
{
	int socketfd, i, dev_num;
	char buf[BUFSIZ];
	const char *addr;
	struct ifconf conf;
	struct ifreq *ifr;
	struct sockaddr_in *sin;
	

	socketfd = socket(PF_INET, SOCK_DGRAM, 0);
	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buf;

	ioctl(socketfd, SIOCGIFCONF, &conf);
	dev_num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;

	for(i=0;i < dev_num;i++)
	{
    	sin = (struct sockaddr_in *)(&ifr->ifr_addr);

    	ioctl(socketfd, SIOCGIFFLAGS, ifr);
    	if(((ifr->ifr_flags & IFF_LOOPBACK) == 0) && (ifr->ifr_flags & IFF_UP)&&(strcmp(ifr->ifr_name,HMP_IB_DEVICE)==0))
    	{
        	INFO_LOG("%s %s",
            	ifr->ifr_name,
            	inet_ntoa(sin->sin_addr));
			addr=inet_ntoa(sin->sin_addr);
			break;
    	}
    	ifr++;
	}

	if(i!=dev_num){
		for(i=0;i<config->node_cnt;i++){
			if(!strcmp(config->node_infos[i].addr, addr)){
				config->curnode_id=i;
				INFO_LOG("curnode id %d",i);
				return ;
			}
		}
	}
	
	ERROR_LOG("don't have rdma device or config.xml exist error.");
	exit(-1);		
}

int hmp_config_init(struct hmp_config *config)
{
	const char *config_file=HMP_CONFIG_FILE;
	int index=0;
	xmlDocPtr config_doc;
	xmlNodePtr curnode;
	
	config_doc=xmlParseFile(config_file);
	if(!config_doc){
		ERROR_LOG("xml parse file error.");
		return -1;
	}

	curnode=xmlDocGetRootElement(config_doc);
	if(!curnode){
		ERROR_LOG("xml doc get root element error.");
		goto cleandoc;
	}

	if(xmlStrcmp(curnode->name, (const xmlChar *)HMP_NODES_INFO_STR)){
		ERROR_LOG("xml root node is not nodes_info.");
		goto cleandoc;
	}
	
	config->node_cnt=0;
	config->curnode_id=-1;
	curnode=curnode->xmlChildrenNode;
	while(curnode){
		if(!xmlStrcmp(curnode->name, (const xmlChar *)HMP_NODE_STR)){
			hmp_parse_node(config, index, config_doc, curnode);
			config->node_cnt++;
			index++;
		}
		curnode=curnode->next;
	}
	
	hmr_print_nodes_info(config);
	xmlFreeDoc(config_doc);
	hmp_set_curnode_id(config);
	return 0;
	
cleandoc:
	xmlFreeDoc(config_doc);
	return -1;
}
