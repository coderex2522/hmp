#ifndef HMP_BASE_H
#define HMP_BASE_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <linux/list.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#include "hmp_rbtree.h"

#define HMP_NODE_NUM 8
#define HMP_TASK_SIZE 40

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#define min(a,b) (a>b?b:a)

struct hmp_config;
struct hmp_context;
struct hmp_node;


enum hmp_log_level{
	HMP_LOG_LEVEL_ERROR,
	HMP_LOG_LEVEL_WARN,
	HMP_LOG_LEVEL_INFO,
	HMP_LOG_LEVEL_DEBUG,
	HMP_LOG_LEVEL_TRACE,
	HMP_LOG_LEVEL_LAST
};

enum hmp_msg_type{
	HMP_MSG_MR,
	HMP_MSG_NORMAL,
	HMP_MSG_READ,
	HMP_MSG_WRITE,
	HMP_MSG_FINISH,
	HMP_MSG_DONE
};

struct hmp_msg{
	enum hmp_msg_type msg_type;
	int  data_size;
	void *data;
};

void* hmp_malloc(int length);

void hmp_free(void *addr, int length);

int hmp_read(void *local_dst, void *hm_src, int length);

int hmp_write(void *hm_dst, void *local_src, int length);

void hmp_print_addr(void *addr, int length);

#endif
