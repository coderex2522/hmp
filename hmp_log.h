#ifndef HMP_LOG_H
#define HMP_LOG_H

extern enum hmp_log_level global_log_level;

extern void hmp_log_impl(const char *file, unsigned line, const char *func,
					unsigned log_level, const char *fmt, ...);


#define hmp_log(level, fmt, ...)\
	do{\
		if(level < HMP_LOG_LEVEL_LAST && level<=global_log_level)\
			hmp_log_impl(__FILE__, __LINE__, __func__,\
							level, fmt, ## __VA_ARGS__);\
	}while(0)



#define	ERROR_LOG(fmt, ...)		hmp_log(HMP_LOG_LEVEL_ERROR, fmt, \
									## __VA_ARGS__)
#define	WARN_LOG(fmt, ...) 		hmp_log(HMP_LOG_LEVEL_WARN, fmt, \
									## __VA_ARGS__)
#define INFO_LOG(fmt, ...)		hmp_log(HMP_LOG_LEVEL_INFO, fmt, \
									## __VA_ARGS__)	
#define DEBUG_LOG(fmt, ...)		hmp_log(HMP_LOG_LEVEL_DEBUG, fmt, \
									## __VA_ARGS__)
#define TRACE_LOG(fmt, ...)		hmp_log(HMP_LOG_LEVEL_TRACE, fmt,\
									## __VA_ARGS__)
#endif