#ifndef HMP_TRANSPORT_H
#define HMP_TRANSPORT_H

#define ADDR_RESOLVE_TIMEOUT 500
#define ROUTE_RESOLVE_TIMEOUT 500

/*set the max send/recv wr num*/
#define MAX_SEND_WR 128
#define MAX_RECV_WR 128
#define MAX_SEND_SGE 4

/*call ibv_create_cq alloc size--second parameter*/
#define CQE_ALLOC_SIZE 20

enum hmp_transport_state {
	HMP_TRANSPORT_STATE_INIT,
	HMP_TRANSPORT_STATE_LISTEN,
	HMP_TRANSPORT_STATE_CONNECTING,
	HMP_TRANSPORT_STATE_CONNECTED,
	HMP_TRANSPORT_STATE_DISCONNECTED,
	HMP_TRANSPORT_STATE_RECONNECT,
	HMP_TRANSPORT_STATE_CLOSED,
	HMP_TRANSPORT_STATE_DESTROYED,
	HMP_TRANSPORT_STATE_ERROR
};

struct hmp_device{
	struct list_head dev_list_entry;
	struct ibv_context	*verbs;
	struct ibv_pd	*pd;
	struct ibv_device_attr device_attr;
};

struct hmp_cq{
	struct ibv_cq	*cq;
	struct ibv_comp_channel	*comp_channel;
	struct hmp_device *device;
	
	/*add the fd of comp_channel into the ctx*/
	struct hmp_context *ctx;
};

struct hmp_region{
	void *send_addr;
	void *recv_addr;
	int cur_recv;
};

struct hmp_transport{
	struct sockaddr_in	peer_addr;
	struct sockaddr_in local_addr;
	
	void *nodeptr;
	int node_id;
	enum hmp_transport_state trans_state;
	struct hmp_context *ctx;
	struct hmp_device *device;
	struct hmp_cq *hcq;
	struct ibv_qp	*qp;
	struct rdma_event_channel *event_channel;
	struct rdma_cm_id	*cm_id;
	struct hmp_region region;
};


void hmp_transport_init();

void hmp_transport_release();

struct hmp_transport *hmp_transport_create(void *nodeptr, struct hmp_context *ctx);

int hmp_transport_listen(struct hmp_transport *rdma_trans, int listen_port);

int hmp_transport_connect(struct hmp_transport* rdma_trans,
								const char *url, int port);
void hmp_post_recv(struct hmp_transport *rdma_trans);

void hmp_post_send(struct hmp_transport *rdma_trans, struct hmp_msg *msg);

void hmp_rdma_read(struct hmp_transport *rdma_trans, void *local_addr, void *remote_addr, int length);

void hmp_rdma_write(struct hmp_transport *rdma_trans, void *local_addr, void *remote_addr, int length);

#endif
