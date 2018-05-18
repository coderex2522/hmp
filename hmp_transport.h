#ifndef HMP_TRANSPORT_H
#define HMP_TRANSPORT_H

enum hmp_transport_state {
	HMP_TRANSPORT_STATE_INIT,
	HMP_TRANSPORT_STATE_LISTEN,
	HMP_TRANSPORT_STATE_CONNECTING,
	HMP_TRANSPORT_STATE_CONNECTED,
	/* when the two sides exchange the info of one sided RDMA,
	 * the trans state will from CONNECTED to S-R-DCONNECTED
	 */
	HMP_TRANSPORT_STATE_SCONNECTED,
	HMP_TRANSPORT_STATE_RCONNECTED,
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

struct hmp_transport{
	enum hmp_transport_state trans_state;
	struct hmp_context *ctx;
	struct rdma_event_channel *event_channel;
};


void hmp_transport_init();

void hmp_transport_release();

struct hmp_transport *hmp_transport_create(struct hmp_context *ctx);

#endif
