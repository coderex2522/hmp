#ifndef HMP_TASK_H
#define HMP_TASK_H

struct hmp_sge{
	void *addr;
	int length;
};

enum hmp_task_type{
	HMP_TASK_MR
};
	
struct hmp_task{
	enum hmp_task_type type;
	struct hmp_sge sge;
	struct hmp_transport *rdma_trans;
};

struct hmp_task *hmp_send_task_create(struct hmp_transport *rdma_trans, struct hmp_msg *msg);
struct hmp_task *hmp_recv_task_create(struct hmp_transport *rdma_trans, int size);

#endif
