#ifndef HMP_BASE_H
#define HMP_BASE_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <linux/list.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#define min(a,b) (a>b?b:a)

struct hmp_config;
struct hmp_context;
//struct hmp_transport;
struct hmp_node;


enum hmp_log_level{
	HMP_LOG_LEVEL_ERROR,
	HMP_LOG_LEVEL_WARN,
	HMP_LOG_LEVEL_INFO,
	HMP_LOG_LEVEL_DEBUG,
	HMP_LOG_LEVEL_TRACE,
	HMP_LOG_LEVEL_LAST
};



void* hmp_malloc(int length);

void hmp_free(void *addr, int length);

int hmp_read(void *dst, void *src, int length);

int hmp_write(void *dst, void *src, int length);
#endif
