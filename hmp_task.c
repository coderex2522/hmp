#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_transport.h"
#include "hmp_task.h"

struct hmp_task *hmp_send_task_create(struct hmp_transport *rdma_trans, struct hmp_msg *msg)
{
	struct hmp_task *task;

	task=(struct hmp_task*)malloc(sizeof(struct hmp_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	task->rdma_trans=rdma_trans;
	task->sge.addr=rdma_trans->region.send_addr;
	task->sge.length=sizeof(enum hmp_msg_type)+sizeof(int)+msg->data_size;
	task->type=HMP_TASK_MR;

	memcpy(task->sge.addr, &msg->msg_type, sizeof(enum hmp_msg_type));
	memcpy(task->sge.addr+sizeof(enum hmp_msg_type), &msg->data_size, sizeof(int));
	memcpy(task->sge.addr+sizeof(enum hmp_msg_type)+sizeof(int), msg->data, msg->data_size);

	return task;
}

struct hmp_task *hmp_recv_task_create(struct hmp_transport *rdma_trans, int size)
{
	struct hmp_task *task;

	task=(struct hmp_task*)malloc(sizeof(struct hmp_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	task->rdma_trans=rdma_trans;
	task->sge.addr=rdma_trans->region.recv_addr+rdma_trans->region.cur_recv*HMP_TASK_SIZE;
	task->sge.length=size;
	rdma_trans->region.cur_recv=(rdma_trans->region.cur_recv+1)%3;
	task->type=HMP_TASK_MR;

	return task;
}

