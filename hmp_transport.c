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
#include "hmp_mem.h"
#include "hmp_task.h"

pthread_once_t init_pthread=PTHREAD_ONCE_INIT;
pthread_once_t release_pthread=PTHREAD_ONCE_INIT;
extern struct hmp_node curnode;//define in the hmp_node.c

void hmp_print_addr(void *addr, int length)
{
	char *caddr=(char*)addr;
	int i;
	for(i=0;i<length;i++)
		printf("%d ",*(caddr+i));
	printf("\n");
}
/**
 *these functions is used for init devices.
 *1.search device
 *2.init device(query device attr,alloc pd,init ibv_context)
 */
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
	const char *dev_name;
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
		dev_name=ibv_get_device_name(ctx_list[i]->device);
		if(strcmp(dev_name,"siw_lo")==0){
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
		else{
			list_add(&dev->dev_list_entry,&curnode.dev_list);
			INFO_LOG("RDMA device [%d]: name= %s allocate success.",
								i, ibv_get_device_name(ctx_list[i]->device));
		}
	}
	rdma_free_devices(ctx_list);
}

void hmp_transport_init()
{
	pthread_once(&init_pthread, hmp_device_init_handle);
}
/**
 *these functions is used for free devices.
 *1.traverse devices list
 *2.free device(dealloc pd,free dev)
 */
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

void hmp_post_recv(struct hmp_transport *rdma_trans)
{
	struct ibv_recv_wr recv_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct hmp_task *task;
	int err=0;
	
	task=hmp_recv_task_create(rdma_trans, HMP_TASK_SIZE, true);
	if(!task){
		ERROR_LOG("recv task create error");
		return ;
	}
	
	recv_wr.wr_id=(uintptr_t)task;
	recv_wr.next=NULL;
	recv_wr.sg_list=&sge;
	recv_wr.num_sge=1;

	sge.addr=(uintptr_t)task->sge.addr;
	sge.length=task->sge.length;
	sge.lkey=task->sge.lkey;
	
	err=ibv_post_recv(rdma_trans->qp, &recv_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv_post_recv error");
	}
}

static void hmp_pre_post_recv(struct hmp_transport *rdma_trans)
{
	int i=0;
	for(i=0;i<2;i++)
		hmp_post_recv(rdma_trans);
}

static struct hmp_device *hmp_device_lookup(void *nodeptr, struct ibv_context *verbs)
{
	struct hmp_device *device;
	struct hmp_node *curnode=(struct hmp_node *)nodeptr;
	
	list_for_each_entry(device, &curnode->dev_list, dev_list_entry){
		if(device->verbs==verbs){
			return device;
		}
	}

	return NULL;
}


static int on_cm_addr_resolved(struct rdma_cm_event *event, struct hmp_transport *rdma_trans)
{
	int retval=0;

	rdma_trans->device=hmp_device_lookup(rdma_trans->nodeptr, rdma_trans->cm_id->verbs);
	if(!rdma_trans->device){
		ERROR_LOG("not found the hmr device.");
		return -1;
	}
	
	retval=rdma_resolve_route(rdma_trans->cm_id, ROUTE_RESOLVE_TIMEOUT);
	if(retval){
		ERROR_LOG("RDMA resolve route error.");
		return retval;
	}

	return retval;
}

const char *hmp_ibv_wc_opcode_str(enum ibv_wc_opcode opcode)
{
	switch (opcode) {
	case IBV_WC_SEND:		return "IBV_WC_SEND";
	case IBV_WC_RDMA_WRITE:		return "IBV_WC_RDMA_WRITE";
	case IBV_WC_RDMA_READ:		return "IBV_WC_RDMA_READ";
	case IBV_WC_COMP_SWAP:		return "IBV_WC_COMP_SWAP";
	case IBV_WC_FETCH_ADD:		return "IBV_WC_FETCH_ADD";
	case IBV_WC_BIND_MW:		return "IBV_WC_BIND_MW";
	/* recv-side: inbound completion */
	case IBV_WC_RECV:		return "IBV_WC_RECV";
	case IBV_WC_RECV_RDMA_WITH_IMM: return "IBV_WC_RECV_RDMA_WITH_IMM";
	default:			return "IBV_WC_UNKNOWN";
	};
}

static void hmp_handle_mr_msg(struct hmp_transport *rdma_trans, struct hmp_msg *msg)
{
	struct hmp_node_info *peer_node_info=&curnode.config.node_infos[rdma_trans->node_id];
	peer_node_info->dram_mr.addr=*(void**)(msg->data);
	peer_node_info->dram_mr.length=*(size_t*)(msg->data+sizeof(void*));
	peer_node_info->dram_mr.rkey=*(uint32_t*)(msg->data+sizeof(void*)+sizeof(size_t));

	INFO_LOG("node_id %d addr %p length %d rkey %ld",
		rdma_trans->node_id,
		peer_node_info->dram_mr.addr,
		peer_node_info->dram_mr.length,
		peer_node_info->dram_mr.rkey);

	rdma_trans->trans_state=HMP_TRANSPORT_STATE_CONNECTED;
}

static void hmp_wc_success_handler(struct ibv_wc *wc)
{
	struct hmp_transport *rdma_trans;
	struct hmp_task *task;
	struct hmp_msg msg;

	task=(struct hmp_task*)(uintptr_t)wc->wr_id;
	rdma_trans=task->rdma_trans;

	if(wc->opcode==IBV_WC_SEND||wc->opcode==IBV_WC_RECV){
		msg.msg_type=*(enum hmp_msg_type*)task->sge.addr;
		msg.data_size=*(int *)(task->sge.addr+sizeof(enum hmp_msg_type));
		msg.data=task->sge.addr+sizeof(enum hmp_msg_type)+sizeof(int);
	}
	
	switch (wc->opcode)
	{
	case IBV_WC_SEND:
		break;
	case IBV_WC_RECV:
		if(msg.msg_type==HMP_MSG_INIT)
			hmp_handle_mr_msg(rdma_trans, &msg);
		break;
	case IBV_WC_RDMA_WRITE:
		break;
	case IBV_WC_RDMA_READ:
		task->type=HMP_TASK_DONE;
		break;
	case IBV_WC_RECV_RDMA_WITH_IMM:
		break;
	default:
		ERROR_LOG("unknown opcode:%s",
				hmp_ibv_wc_opcode_str(wc->opcode));
		break;
	}
	
}

static void hmp_wc_error_handler(struct ibv_wc *wc)
{
	if(wc->status==IBV_WC_WR_FLUSH_ERR)
		INFO_LOG("work request flush error.");
	else
		ERROR_LOG("wc status [%s] is error.",
				ibv_wc_status_str(wc->status));

}

static void hmp_cq_comp_channel_handler(int fd, void *data)
{
	struct hmp_cq *hcq=(struct hmp_cq*)data;
	struct ibv_cq *cq;
	void *cq_ctx;
	struct ibv_wc wc;
	int err=0;

	err=ibv_get_cq_event(hcq->comp_channel, &cq, &cq_ctx);
	if(err){
		ERROR_LOG("ibv get cq event error.");
		return ;
	}

	ibv_ack_cq_events(hcq->cq, 1);
	err=ibv_req_notify_cq(hcq->cq, 0);
	if(err){
		ERROR_LOG("ibv req notify cq error.");
		return ;
	}
	
	while(ibv_poll_cq(hcq->cq,1,&wc)){
		if(wc.status==IBV_WC_SUCCESS)
			hmp_wc_success_handler(&wc);
		else
			hmp_wc_error_handler(&wc);

	}
}

static struct hmp_cq* hmp_cq_get(struct hmp_device *device, struct hmp_context *ctx)
{
	struct hmp_cq *hcq;
	int retval,alloc_size,flags=0;

	hcq=(struct hmp_cq*)calloc(1,sizeof(struct hmp_cq));
	if(!hcq){
		ERROR_LOG("allocate the memory of struct hmp_cq error.");
		return NULL;
	}
	
	hcq->comp_channel=ibv_create_comp_channel(device->verbs);
	if(!hcq->comp_channel){
		ERROR_LOG("rdma device %p create comp channel error.",device);
		goto cleanhcq;
	}

	flags=fcntl(hcq->comp_channel->fd, F_GETFL, 0);
	if(flags!=-1)
		flags=fcntl(hcq->comp_channel->fd, F_SETFL, flags|O_NONBLOCK);

	if(flags==-1){
		ERROR_LOG("set hcq comp channel fd nonblock error.");
		goto cleanchannel;
	}

	hcq->ctx=ctx;
	retval=hmp_context_add_event_fd(hcq->ctx,
								EPOLLIN,
								hcq->comp_channel->fd, hcq,
						 		hmp_cq_comp_channel_handler);
	if(retval){
		ERROR_LOG("context add comp channel fd error.");
		goto cleanchannel;		
	}

	alloc_size=min(CQE_ALLOC_SIZE, device->device_attr.max_cqe);
	hcq->cq=ibv_create_cq(device->verbs, alloc_size, hcq, hcq->comp_channel, 0);
	if(!hcq->cq){
		ERROR_LOG("ibv create cq error.");
		goto cleaneventfd;
	}

	retval=ibv_req_notify_cq(hcq->cq, 0);
	if(retval){
		ERROR_LOG("ibv req notify cq error.");
		goto cleaneventfd;
	}

	hcq->device=device;
	return hcq;
	
cleaneventfd:
	hmp_context_del_event_fd(ctx, hcq->comp_channel->fd);
	
cleanchannel:
	ibv_destroy_comp_channel(hcq->comp_channel);
	
cleanhcq:
	free(hcq);
	hcq=NULL;
	
	return hcq;
}

static int hmp_qp_create(struct hmp_transport *rdma_trans)
{
	int retval=0;
	struct ibv_qp_init_attr qp_init_attr;
	struct hmp_cq *hcq;
	
	hcq=hmp_cq_get(rdma_trans->device, rdma_trans->ctx);
	if(!hcq){
		ERROR_LOG("hmr cq get error.");
		return -1;
	}
	
	memset(&qp_init_attr,0,sizeof(qp_init_attr));
	qp_init_attr.qp_context=rdma_trans;
	qp_init_attr.qp_type=IBV_QPT_RC;
	qp_init_attr.send_cq=hcq->cq;
	qp_init_attr.recv_cq=hcq->cq;

	qp_init_attr.cap.max_send_wr=MAX_SEND_WR;
	qp_init_attr.cap.max_send_sge=min(rdma_trans->device->device_attr.max_sge,
									MAX_SEND_SGE);
	
	qp_init_attr.cap.max_recv_wr=MAX_RECV_WR;
	qp_init_attr.cap.max_recv_sge=1;

	retval=rdma_create_qp(rdma_trans->cm_id,
						rdma_trans->device->pd,
						&qp_init_attr);
	if(retval){
		ERROR_LOG("rdma create qp error.");
		goto cleanhcq;
	}
	
	rdma_trans->qp=rdma_trans->cm_id->qp;
	rdma_trans->hcq=hcq;
	
	return retval;
/*if provide for hcq with per context,there not delete hcq.*/
cleanhcq:
	free(hcq);
	return retval;
}

static void hmp_qp_release(struct hmp_transport* rdma_trans)
{
	if(rdma_trans->qp){
		ibv_destroy_qp(rdma_trans->qp);
		ibv_destroy_cq(rdma_trans->hcq->cq);
		hmp_context_del_event_fd(rdma_trans->ctx, rdma_trans->hcq->comp_channel->fd);
		free(rdma_trans->hcq);
		rdma_trans->hcq=NULL;
	}
}

static int on_cm_route_resolved(struct rdma_cm_event *event, struct hmp_transport *rdma_trans)
{
	struct rdma_conn_param conn_param;
	int retval=0;
	
	retval=hmp_qp_create(rdma_trans);
	if(retval){
		ERROR_LOG("hmr qp create error.");
		return retval;
	}
	hmp_pre_post_recv(rdma_trans);
	
	memset(&conn_param, 0, sizeof(conn_param));
	conn_param.retry_count=3;
	conn_param.rnr_retry_count=7;

	conn_param.responder_resources = 1;
		//rdma_trans->device->device_attr.max_qp_rd_atom;
	conn_param.initiator_depth = 1;
		//rdma_trans->device->device_attr.max_qp_init_rd_atom;
	
	INFO_LOG("RDMA Connect.");
	
	retval=rdma_connect(rdma_trans->cm_id, &conn_param);
	if(retval){
		ERROR_LOG("rdma connect error.");
		goto cleanqp;
	}

	return retval;
	
cleanqp:
	hmp_qp_release(rdma_trans);
	rdma_trans->ctx->stop=1;
	rdma_trans->trans_state=HMP_TRANSPORT_STATE_ERROR;
	return retval;
}

static int on_cm_connect_request(struct rdma_cm_event *event, struct hmp_transport *rdma_trans)
{
	struct hmp_transport *new_trans;
	struct rdma_conn_param conn_param;
	int retval=0,i;
	char *peer_addr;
	
	INFO_LOG("event id %p rdma_trans cm_id %p event_listenid %p",event->id,rdma_trans->cm_id,event->listen_id);
	 
	new_trans=hmp_transport_create(rdma_trans->nodeptr, rdma_trans->ctx);
	if(!new_trans){
		ERROR_LOG("rdma trans process connect request error.");
		return -1;
	}
	
	new_trans->cm_id=event->id;
	event->id->context=new_trans;
	new_trans->device=hmp_device_lookup(new_trans->nodeptr, event->id->verbs);
	if(!new_trans->device){
		ERROR_LOG("can't find the rdma device.");
		return -1;
	}
	
	retval=hmp_qp_create(new_trans);
	if(retval){
		ERROR_LOG("hmp qp create error.");
		return retval;
	}
	
	peer_addr=inet_ntoa(event->id->route.addr.dst_sin.sin_addr);
	for(i=0;i<curnode.config.curnode_id;i++){
		if(strcmp(curnode.config.node_infos[i].addr,peer_addr)==0){
			INFO_LOG("find addr trans %d %s",i,peer_addr);
			curnode.connect_trans[i]=new_trans;
			new_trans->node_id=i;
			new_trans->region.cur_recv=0;
			new_trans->region.send_addr=curnode.dram_mempool->base_addr+i*4*HMP_TASK_SIZE;
			new_trans->region.recv_addr=new_trans->region.send_addr+HMP_TASK_SIZE;
			break;
		}
	}
	hmp_pre_post_recv(new_trans);
	
	memset(&conn_param, 0, sizeof(conn_param));
	retval=rdma_accept(new_trans->cm_id, &conn_param);
	if(retval){
		ERROR_LOG("rdma accept error.");
		return -1;
	}
	new_trans->trans_state=HMP_TRANSPORT_STATE_CONNECTING;
	return retval;
}

static void hmp_build_init_msg(struct hmp_msg *msg)
{
	msg->msg_type=HMP_MSG_INIT;
	msg->data_size=sizeof(void*)+sizeof(size_t)+sizeof(uint32_t);
	msg->data=malloc(msg->data_size);
	
	memcpy(msg->data,
		&curnode.dram_mempool->base_addr, sizeof(void*));
	memcpy(msg->data+sizeof(void*), 
		&curnode.dram_mempool->mr->length, sizeof(size_t));
	memcpy(msg->data+sizeof(void*)+sizeof(size_t), 
		&curnode.dram_mempool->mr->rkey, sizeof(uint32_t));
}

static int on_cm_established(struct rdma_cm_event *event, struct hmp_transport *rdma_trans)
{
	int retval=0;
	struct hmp_msg msg;
	
	memcpy(&rdma_trans->local_addr,
		&rdma_trans->cm_id->route.addr.src_sin,
		sizeof(rdma_trans->local_addr));

	memcpy(&rdma_trans->peer_addr,
		&rdma_trans->cm_id->route.addr.dst_sin,
		sizeof(rdma_trans->peer_addr));

	hmp_build_init_msg(&msg);
	hmp_post_send(rdma_trans, &msg);
	
	return retval;
}

static int on_cm_disconnected(struct rdma_cm_event *event, struct hmp_transport *rdma_trans)
{
	int retval=0;
	
	rdma_destroy_qp(rdma_trans->cm_id);

	hmp_context_del_event_fd(rdma_trans->ctx, rdma_trans->hcq->comp_channel->fd);
	hmp_context_del_event_fd(rdma_trans->ctx, rdma_trans->event_channel->fd);

	INFO_LOG("rdma disconnected success.");
	return retval;
}

static int hmp_handle_ec_event(struct rdma_cm_event *event)
{
	int retval=0;
	struct hmp_transport *rdma_trans;
	rdma_trans=(struct hmp_transport*)event->id->context;
		
	INFO_LOG("cm event [%s],status:%d",
			rdma_event_str(event->event),event->status);
	
	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		retval=on_cm_addr_resolved(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		retval=on_cm_route_resolved(event,rdma_trans);
		break;
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
	};
	
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
		flags=fcntl(rdma_trans->event_channel->fd,
				F_SETFL, flags|O_NONBLOCK);

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


struct hmp_transport *hmp_transport_create(void *nodeptr, struct hmp_context *ctx)
{
	struct hmp_transport *rdma_trans;
	int err=0;
	
	rdma_trans=(struct hmp_transport*)calloc(1,sizeof(struct hmp_transport));
	if(!rdma_trans){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	rdma_trans->trans_state=HMP_TRANSPORT_STATE_INIT;
	rdma_trans->ctx=ctx;
	rdma_trans->nodeptr=nodeptr;
	
	err=hmp_event_channel_create(rdma_trans);
	if(err){
		ERROR_LOG("hmp event channel create error.");
		goto cleantrans;
	}
	
	return rdma_trans;
	
cleantrans:
	free(rdma_trans);
	return NULL;
}

int hmp_transport_listen(struct hmp_transport *rdma_trans, int listen_port)
{
	int retval=0, backlog;
	struct sockaddr_in addr;
	
	retval=rdma_create_id(rdma_trans->event_channel,
						&rdma_trans->cm_id,
						rdma_trans, RDMA_PS_TCP);
	if(retval){
		ERROR_LOG("rdma create id error.");
		return retval;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(listen_port);
	
	retval=rdma_bind_addr(rdma_trans->cm_id,
						(struct sockaddr*)&addr);
	if(retval){
		ERROR_LOG("rdma bind addr error.");
		goto cleanid;
	}

	backlog=10;
	retval=rdma_listen(rdma_trans->cm_id, backlog);
	if(retval){
		ERROR_LOG("rdma listen error.");
		goto cleanid;
	}

	rdma_trans->trans_state=HMP_TRANSPORT_STATE_LISTEN;
	INFO_LOG("rdma listening on port %d",
			ntohs(rdma_get_src_port(rdma_trans->cm_id)));

	return retval;
	
cleanid:
	rdma_destroy_id(rdma_trans->cm_id);
	rdma_trans->cm_id=NULL;
	
	return retval;
}

static int hmp_port_uri_init(struct hmp_transport *rdma_trans,
						const char *url,int port)
{
	struct sockaddr_in peer_addr;
	int retval=0;

	memset(&peer_addr,0,sizeof(peer_addr));
	peer_addr.sin_family=AF_INET;//PF_INET=AF_INET
	peer_addr.sin_port=htons(port);

	retval=inet_pton(AF_INET, url, &peer_addr.sin_addr);
	if(retval<=0){
		ERROR_LOG("IP Transfer Error.");
		goto exit;
	}
	memcpy(&rdma_trans->peer_addr, &peer_addr, sizeof(struct sockaddr_in));
exit:
	return retval;
}

int hmp_transport_connect(struct hmp_transport* rdma_trans,
								const char *url, int port)
{
	int retval=0;
	if(!url||!port){
		ERROR_LOG("Url or port input error.");
		return -1;
	}

	retval=hmp_port_uri_init(rdma_trans, url, port);
	if(retval<0){
		ERROR_LOG("rdma init port uri error.");
		return retval;
	}

	/*rdma_cm_id dont init the rdma_cm_id's verbs*/
	retval=rdma_create_id(rdma_trans->event_channel,
					&rdma_trans->cm_id,rdma_trans,RDMA_PS_TCP);
	if(retval){
		ERROR_LOG("rdma create id error.");
		goto cleanrdmatrans;
	}
	retval=rdma_resolve_addr(rdma_trans->cm_id, NULL,
					(struct sockaddr*)&rdma_trans->peer_addr,
					ADDR_RESOLVE_TIMEOUT);
	if(retval){
		ERROR_LOG("RDMA Device resolve addr error.");
		goto cleancmid;
	}
	rdma_trans->trans_state=HMP_TRANSPORT_STATE_CONNECTING;
	return retval;
	
cleancmid:
	rdma_destroy_id(rdma_trans->cm_id);

cleanrdmatrans:
	rdma_trans->cm_id=NULL;
	
	return retval;
}

void hmp_post_send(struct hmp_transport *rdma_trans, struct hmp_msg *msg)
{
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct hmp_task *task;
	int err=0;
		
	task=hmp_send_task_create(rdma_trans, msg, true);
	if(!task){
		ERROR_LOG("send task create error.");
		return ;
	}
	
	memset(&send_wr,0,sizeof(send_wr));

	send_wr.wr_id=(uintptr_t)task;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.opcode=IBV_WR_SEND;
	send_wr.send_flags=IBV_SEND_SIGNALED;

	sge.addr=(uintptr_t)task->sge.addr;
	sge.length=task->sge.length;
	sge.lkey=task->sge.lkey;
	
	err=ibv_post_send(rdma_trans->qp, &send_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv_post_send error.");
	}
}

static int hmp_transport_check_state(struct hmp_transport *rdma_trans)
{
	while(rdma_trans->trans_state<HMP_TRANSPORT_STATE_CONNECTED){
		;
	}
	if(rdma_trans->trans_state>HMP_TRANSPORT_STATE_CONNECTED){
		ERROR_LOG("rdma transport exits error.");
		return -1;
	}

	return 0;
}

void hmp_rdma_write(struct hmp_transport *rdma_trans, void *local_addr, void *remote_addr, int length)
{
	struct hmp_task *task;
	struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;
	int err=0;

	if(hmp_transport_check_state(rdma_trans)==-1)
		return ;
	

	task=hmp_write_task_create(rdma_trans, local_addr, length, true);
	if(!task){
		ERROR_LOG("write task create error.");
		return ;
	}
	
    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)task;
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = (uintptr_t)remote_addr;
    wr.wr.rdma.rkey = curnode.config.node_infos[rdma_trans->node_id].dram_mr.rkey;

    sge.addr = (uintptr_t)task->sge.addr;
    sge.length = task->sge.length;
    sge.lkey = task->sge.lkey;
	
    err=ibv_post_send(rdma_trans->qp, &wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv_post_send error.");
	}
}


void hmp_rdma_read(struct hmp_transport *rdma_trans, void *local_addr, void *remote_addr, int length)
{
	struct hmp_task *read_task;
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	int err=0;

	if(hmp_transport_check_state(rdma_trans)==-1)
		return ;

	read_task=hmp_read_task_create(rdma_trans, local_addr, length, true);
	if(!read_task){
		ERROR_LOG("create task error.");
		return ;
	}
	
	memset(&send_wr,0,sizeof(struct ibv_send_wr));

	send_wr.wr_id=(uintptr_t)read_task;
	send_wr.opcode=IBV_WR_RDMA_READ;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.send_flags=IBV_SEND_SIGNALED;
	send_wr.wr.rdma.remote_addr=(uintptr_t)remote_addr;
	send_wr.wr.rdma.rkey=curnode.config.node_infos[rdma_trans->node_id].dram_mr.rkey;

	sge.addr=(uintptr_t)read_task->sge.addr;
	sge.length=read_task->sge.length;
	sge.lkey=read_task->sge.lkey;

	err=ibv_post_send(rdma_trans->qp, &send_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv_post_send error");
	}

	while(read_task->type!=HMP_TASK_DONE);
	INFO_LOG("read content is %s",local_addr);
}

