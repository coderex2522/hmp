#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_config.h"
#include "hmp_context.h"
#include "hmp_transport.h"
#include "hmp_node.h"

void hmp_node_init()
{
	hmp_config_init(&curnode.config);
	curnode.ctx=hmp_context_create();
	hmp_transport_init();
}


void hmp_node_release()
{
	pthread_join(curnode.ctx->epoll_thread, NULL);
	hmp_transport_release();
}