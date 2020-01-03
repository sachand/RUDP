#pragma once

#include <stdio.h>
#include <stdint.h>
#include <time.h>

/*
 * Timestamp Format: DD-MM-YYYY HH:MM:SS
 */
#define TIMESTAMP_FORMAT "%.2d-%.2d-%.4d %.2d:%.2d:%.2d"
#define TIMESTAMP_LENGTH 20

#ifdef CLOCKS_PER_SEC
#define HZ CLOCKS_PER_SEC
#else
#define HZ CLK_TCK
#endif

#define CLOCK_GRANULARITY_MS (1000 / HZ)

uint64_t
get_current_system_time_millis();

char *
get_timestamp();

void
copy_timestamp(char *timestamp_string);

struct timespec
get_abstime_after (uint32_t timeout_ms);
