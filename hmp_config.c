#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_config.h"


#define HMP_NODES_INFO_STR "nodes_info"
#define HMP_NODE_STR "node"
#define HMP_ID_STR "id"
#define HMP_ADDR_STR "addr"
#define HMP_PORT_STR "port"

static int hmp_parse_node(int index, xmlDocPtr doc, xmlNodePtr curnode)
{
	xmlChar *val;

	curnode=curnode->xmlChildrenNode;
	while(curnode!=NULL){
		if(!xmlStrcmp(curnode->name, (const xmlChar *)HMP_ADDR_STR)){
			val=xmlNodeListGetString(doc, curnode->xmlChildrenNode, 1);
			memcpy(global_config.node_infos[index].addr,
				(const void*)val,strlen((const char*)val)+1);
			xmlFree(val);
		}

		if(!xmlStrcmp(curnode->name, (const xmlChar *)HMP_PORT_STR)){
			val=xmlNodeListGetString(doc, curnode->xmlChildrenNode, 1);
			global_config.node_infos[index].port=atoi((const char*)val);
			xmlFree(val);
		}

		curnode=curnode->next;
	}
	return 0;
}

static void hmr_print_nodes_info()
{
	int i;
	for(i=0;i<global_config.node_cnt;i++){
		INFO_LOG("id %d",global_config.node_infos[i].id);
		INFO_LOG("addr %s",global_config.node_infos[i].addr);
		INFO_LOG("port %d",global_config.node_infos[i].port);
	}
}

int hmp_config_init()
{
	const char *config_file=HMP_CONFIG_FILE;
	xmlDocPtr config_doc;
	xmlNodePtr curnode;
	xmlChar *id;
	
	int index=0;
	
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
	global_config.node_cnt=0;
	curnode=curnode->xmlChildrenNode;
	while(curnode){
		if(!xmlStrcmp(curnode->name, (const xmlChar *)HMP_NODE_STR)){
			id=xmlGetProp(curnode, (const xmlChar *)HMP_ID_STR);
			global_config.node_infos[index].id=atoi((const char*)id);
			hmp_parse_node(index,  config_doc, curnode);
			global_config.node_cnt++;
			index++;
		}
		curnode=curnode->next;
	}
	
	hmr_print_nodes_info();
	xmlFreeDoc(config_doc);
	return 0;
	
cleandoc:
	xmlFreeDoc(config_doc);
	return -1;
}


int main()
{
	hmp_config_init();
	return 0;
}
