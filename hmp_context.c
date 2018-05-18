#include "hmp_base.h"
#include "hmp_log.h"
#include "hmp_context.h"

struct hmp_context *hmp_context_create()
{
	struct hmp_context *ctx;
	int retval=0;

	ctx=(struct hmp_context *)calloc(1,sizeof(struct hmp_context));
	if(!ctx){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	ctx->epfd=epoll_create(HMP_EPOLL_SIZE);
	if(ctx->epfd < 0){
		ERROR_LOG("create epoll fd error.");
		goto cleanctx;
	}

	retval=pthread_create(&ctx->epoll_thread, NULL, hmp_context_run, ctx);
	if(retval){
		ERROR_LOG("pthread create error.");
		goto cleanepfd;
	}
	
	INFO_LOG("hmp context create success.");
	return ctx;

cleanepfd:
	close(ctx->epfd);

cleanctx:
	free(ctx);
	
	return NULL;
}

int hmp_context_add_event_fd(struct hmp_context *ctx,
							int events,
							int fd, void *data,
							hmp_event_handler event_handler)
{
	struct epoll_event ee;
	struct hmp_event_data *event_data;
	int retval=0;
	
	event_data=(struct hmp_event_data*)calloc(1,sizeof(struct hmp_event_data));
	if(!event_data){
		ERROR_LOG("allocate memory error.");
		return -1;
	}

	event_data->fd=fd;
	event_data->data=data;
	event_data->event_handler=event_handler;
	
	memset(&ee,0,sizeof(ee));
	ee.events=events;
	ee.data.ptr=event_data;

	retval=epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, fd, &ee);
	if(retval){
		ERROR_LOG("Context add event fd error.");
		free(event_data);
	}
	else
		INFO_LOG("hmp_context add event fd success.");
	
	return retval;
}

int hmp_context_del_event_fd(struct hmp_context *ctx, int fd)
{
	int retval=0;
	retval=epoll_ctl(ctx->epfd,EPOLL_CTL_DEL,fd,NULL);
	if(retval<0)
		ERROR_LOG("hmp_context del event fd error.");
	
	return retval;
}


void *hmp_context_run(void *data)
{
	struct epoll_event events[1024];
	struct hmp_event_data *event_data;
	struct hmp_context *ctx=(struct hmp_context*)data;
	int i,events_nr=0;
	INFO_LOG("hmp_context_run.");
	while(1){
		events_nr=epoll_wait(ctx->epfd,events,ARRAY_SIZE(events),5000);
		if(events_nr>0){
			for(i=0;i<events_nr;i++){
				event_data=(struct hmp_event_data*)events[i].data.ptr;
				event_data->event_handler(event_data->fd,event_data->data);
			}
		}
		if(ctx->stop){
			break;
		}
	}

	return 0;
}


