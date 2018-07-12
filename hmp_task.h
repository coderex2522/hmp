#ifndef HMP_TASK_H
#define HMP_TASK_H

struct hmp_sge{
	void *addr;
	uint32_t length;
	uint32_t lkey;
};

enum hmp_task_type{
	HMP_TASK_INIT,
	HMP_TASK_READ,
	HMP_TASK_WRITE,
	HMP_TASK_DONE
};
	
struct hmp_task{
	enum hmp_task_type type;
	struct hmp_sge sge;
	struct hmp_transport *rdma_trans;
};

struct hmp_task *hmp_send_task_create(struct hmp_transport *rdma_trans, struct hmp_msg *msg, bool is_dram);
struct hmp_task *hmp_recv_task_create(struct hmp_transport *rdma_trans, int size, bool is_dram);

struct hmp_task *hmp_read_task_create(struct hmp_transport *rdma_trans, void *addr, int length, bool is_dram);
struct hmp_task *hmp_write_task_create(struct hmp_transport *rdma_trans, void *addr, int length, bool is_dram);

#endif
