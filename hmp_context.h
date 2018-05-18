#ifndef HMP_CONTEXT_H
#define HMP_CONTEXT_H

#define HMP_EPOLL_SIZE 1024

typedef void (*hmp_event_handler)(int fd, void *data);

struct hmp_event_data{
	int fd;
	void *data;
	hmp_event_handler event_handler;
};

struct hmp_context{
	int epfd;
	int stop;
	pthread_t epoll_thread;
};

struct hmp_context *hmp_context_create();

int hmp_context_add_event_fd(struct hmp_context *ctx,
							int events,
							int fd, void *data,
							hmp_event_handler event_handler);

int hmp_context_del_event_fd(struct hmp_context *ctx, int fd);

void *hmp_context_run(void *data);
#endif
