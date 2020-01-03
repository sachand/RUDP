#include <pthread.h>
#include <sys/syscall.h>

#include "logger.h"
#include "np_time.h"

#include <stdarg.h>

pthread_mutex_t g_write_lock = PTHREAD_MUTEX_INITIALIZER;
FILE *g_write_target;
log_level g_log_level = INFO;

void
__write_log(char *message)
{
	pthread_mutex_lock(&g_write_lock);
		fprintf(g_write_target, "%s\n", message);
		fflush(g_write_target);
	pthread_mutex_unlock(&g_write_lock);
}

char
get_level_prefix(log_level level)
{
	switch(level)
	{
	case WTF:
		return 'F';
	case SHOW:
		return 'S';
	case ERROR:
		return 'E';
	case WARNING:
		return 'W';
	case INFO:
		return 'I';
	case DEBUG:
		return 'D';
	case VERBOSE:
		return 'V';
	default:
		return '\0';
	}
}

uint32_t
append_timestamp(char *message, uint32_t size_left)
{
    char *timestamp_wo_date = get_timestamp();
    timestamp_wo_date += 11; // remove date
	return sprintf(message, "%s", timestamp_wo_date);
}

uint32_t
append_level_prefix(char *message, uint32_t size_left, log_level level)
{
#define LEVEL_PREFIX_LENGTH 4
	if (size_left < LEVEL_PREFIX_LENGTH) { return 0; }

    message[0] = ' ';
	message[1] = get_level_prefix(level);
	message[2] = '\\';
	message[3] = '\0';

	return LEVEL_PREFIX_LENGTH - 1;
}

uint32_t
append_tag(char *message, uint32_t size_left, char *tag)
{
	return sprintf(message, " %*s", -MAX_TAG_LENGTH, tag);
}

uint32_t
append_process_thread_ids(char *message, uint32_t size_left)
{
	int pid = getpid();

#ifdef SYS_gettid
	int tid = syscall(SYS_gettid);
    return sprintf(message, " %5d %5d ", pid, tid);
#else
    return sprintf(message, " %5d %2d ", pid, (int)pthread_self());
#endif
}

void
write_log(log_level level, char *tag, char *format, ...)
{
    g_write_target = stdout;
    if (g_log_level > level) { return; }

    va_list args;
    va_start(args, format);

    int offset = 0;
    char message[MAX_MESSAGE_SIZE_LOG];
    offset += append_timestamp(message + offset, MAX_MESSAGE_SIZE_LOG - offset);
    offset += append_level_prefix(message + offset, MAX_MESSAGE_SIZE_LOG - offset, level);
    offset += append_tag(message + offset, MAX_MESSAGE_SIZE_LOG - offset, tag);
    offset += append_process_thread_ids(message + offset, MAX_MESSAGE_SIZE_LOG - offset);

    vsprintf(message + offset, format, args);
    message[MAX_MESSAGE_SIZE_LOG - 1] = '\0';
    va_end(args);

	__write_log(message);
}
