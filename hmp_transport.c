#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_config.h"
#include "hmp_context.h"
#include "hmp_transport.h"
#include "hmp_node.h"

pthread_once_t init_pthread=PTHREAD_ONCE_INIT;
pthread_once_t release_pthread=PTHREAD_ONCE_INIT;

static struct hmp_device *hmp_device_init(struct ibv_context *verbs)
{
	struct hmp_device *dev;
	int retval=0;
	
	dev=(struct hmp_device*)calloc(1,sizeof(struct hmp_device));
	if(!dev){
		ERROR_LOG("Allocate hmp_device error.");
		return NULL;
	}

	retval=ibv_query_device(verbs, &dev->device_attr);
	if(retval<0){
		ERROR_LOG("rdma query device attr error.");
		goto cleandev;
	}

	dev->pd=ibv_alloc_pd(verbs);
	if(!dev->pd){
		ERROR_LOG("Allocate ibv_pd error.");
		goto cleandev;
	}

	dev->verbs=verbs;
	INIT_LIST_HEAD(&dev->dev_list_entry);

	return dev;
	
cleandev:
	free(dev);

	return NULL;
}

static void hmp_device_init_handle(void)
{
	struct ibv_context **ctx_list;
	struct hmp_device *dev;
	int i,num_devices=0;

	INIT_LIST_HEAD(&curnode.dev_list);
	
	ctx_list=rdma_get_devices(&num_devices);
	if(!ctx_list){
		ERROR_LOG("Failed to get the rdma device list.");
		return ;
	}

	curnode.num_devices=num_devices;
	for(i=0;i<num_devices;i++){
		if(!ctx_list[i]){
			ERROR_LOG("RDMA device [%d] is NULL.",i);
			curnode.num_devices--;
			continue;
		}
		dev=hmp_device_init(ctx_list[i]);
		if(!dev){
			ERROR_LOG("RDMA device [%d]: name= %s allocate error.",
					i, ibv_get_device_name(ctx_list[i]->device));
			curnode.num_devices--;
			continue;
		}
		else
			INFO_LOG("RDMA device [%d]: name= %s allocate success.",
					i, ibv_get_device_name(ctx_list[i]->device));
		list_add(&dev->dev_list_entry,&curnode.dev_list);
	}

	rdma_free_devices(ctx_list);
}

void hmp_transport_init()
{
	pthread_once(&init_pthread,hmp_device_init_handle);
}





static void hmp_device_release(struct hmp_device *dev)
{
	list_del(&dev->dev_list_entry);
	ibv_dealloc_pd(dev->pd);
	free(dev);
}

static void hmp_device_release_handle(void)
{
	struct hmp_device	*dev, *next;

	/* free devices */
	list_for_each_entry_safe(dev, next, &curnode.dev_list, dev_list_entry) {
		list_del_init(&dev->dev_list_entry);
		hmp_device_release(dev);
	}
}

void hmp_transport_release()
{
	pthread_once(&release_pthread, hmp_device_release_handle);
}


static int hmp_handle_ec_event(struct rdma_cm_event *event)
{
	int retval=0;
	//struct hmp_transport *rdma_trans;
	//rdma_trans=(struct hmp_transport*)event->id->context;
		
	INFO_LOG("cm event [%s],status:%d",
			rdma_event_str(event->event),event->status);
	/*
	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		retval=on_cm_addr_resolved(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		retval=on_cm_route_resolved(event,rdma_trans);
		break;
	//server can call the connect request
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		retval=on_cm_connect_request(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		retval=on_cm_established(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		retval=on_cm_disconnected(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_CONNECT_ERROR:
		rdma_trans->ctx->stop=1;
		rdma_trans->trans_state=HMP_TRANSPORT_STATE_ERROR;
	default:
		//occur an error
		retval=-1;
		break;
	};*/
	
	return retval;
}

static void hmp_event_channel_handler(int fd, void *data)
{
	struct rdma_event_channel *ec=(struct rdma_event_channel*)data;
	struct rdma_cm_event *event,event_copy;
	int retval=0;

	event=NULL;
	while ((retval=rdma_get_cm_event(ec,&event))==0){
		memcpy(&event_copy,event,sizeof(*event));
		
		/*
		 * note: rdma ack cm event will clear event content
		 * so need to copy event content into event_copy.
		 */
		rdma_ack_cm_event(event);

		if(hmp_handle_ec_event(&event_copy))
			break;
	}
	
	if(retval&&errno!=EAGAIN)
		ERROR_LOG("rdma get cm event error.");
}


static int hmp_event_channel_create(struct hmp_transport *rdma_trans)
{
	int flags,retval=0;
	
	rdma_trans->event_channel=rdma_create_event_channel();
	if(!rdma_trans->event_channel){
		ERROR_LOG("rdma create event channel error.");
		return -1;
	}

	flags=fcntl(rdma_trans->event_channel->fd, F_GETFL, 0);
	if(flags!=-1)
		flags=fcntl(rdma_trans->event_channel->fd, F_SETFL,
					flags|O_NONBLOCK);

	if(flags==-1){
		retval=-1;
		ERROR_LOG("set event channel nonblock error.");
		goto cleanec;
	}

	hmp_context_add_event_fd(rdma_trans->ctx, EPOLLIN,
						rdma_trans->event_channel->fd,
						rdma_trans->event_channel,
						hmp_event_channel_handler);
	return retval;
	
cleanec:
	rdma_destroy_event_channel(rdma_trans->event_channel);
	return retval;
}


struct hmp_transport *hmp_transport_create(struct hmp_context *ctx)
{
	struct hmp_transport *rdma_trans;
	
	rdma_trans=(struct hmp_transport*)calloc(1,sizeof(struct hmp_transport));
	if(!rdma_trans){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	rdma_trans->trans_state=HMP_TRANSPORT_STATE_INIT;
	rdma_trans->ctx=ctx;
	hmp_event_channel_create(rdma_trans);

	return rdma_trans;
}

