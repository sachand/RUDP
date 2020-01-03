#include "np_time.h"

uint64_t
get_current_system_time_millis()
{
	return (uint64_t)time(NULL) * 1000;
}

char *
get_timestamp()
{
	static char timestamp_string[TIMESTAMP_LENGTH] = {'\0'};
	time_t timer = time(NULL);
	struct tm *current_time = localtime(&timer);
	snprintf(timestamp_string, TIMESTAMP_LENGTH, TIMESTAMP_FORMAT,
			current_time->tm_mday, current_time->tm_mon + 1,
			current_time->tm_year + 1900, current_time->tm_hour,
			current_time->tm_min, current_time->tm_sec);
    return timestamp_string;
}

void
copy_timestamp(char *timestamp_string)
{
	time_t timer = time(NULL);
	struct tm *current_time = localtime(&timer);
	snprintf(timestamp_string, TIMESTAMP_LENGTH, TIMESTAMP_FORMAT,
			current_time->tm_mday, current_time->tm_mon + 1,
			current_time->tm_year + 1900, current_time->tm_hour,
			current_time->tm_min, current_time->tm_sec);
}

struct timespec
get_abstime_after (uint32_t timeout_ms)
{
    long l_time = (long)timeout_ms;
    struct timespec abs_timeout;

    clock_gettime(CLOCK_REALTIME, &abs_timeout);
    long nansec = abs_timeout.tv_nsec + ((timeout_ms % 1000L) * 1000000L);
    abs_timeout.tv_nsec = nansec % 1000000000L;
    abs_timeout.tv_sec += (timeout_ms / 1000L) + (nansec / 1000000000L);

    return abs_timeout;
}
