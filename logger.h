#pragma once

#include <stdio.h>
#include <stdint.h>

typedef enum log_level_t
{
    VERBOSE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    SHOW,
    WTF
} log_level;

#define LOGWTF(_format_, ...)	(write_log(WTF, LOG_TAG, _format_, ## __VA_ARGS__))
#define LOGS(_format_, ...)	(write_log(SHOW, LOG_TAG, _format_, ## __VA_ARGS__))
#define LOGE(_format_, ...)	(write_log(ERROR, LOG_TAG, _format_, ## __VA_ARGS__))
#define LOGW(_format_, ...)	(write_log(WARNING, LOG_TAG, _format_, ## __VA_ARGS__))
#define LOGI(_format_, ...)	(write_log(INFO, LOG_TAG, _format_, ## __VA_ARGS__))
#define LOGD(_format_, ...)	(write_log(DEBUG, LOG_TAG, _format_, ## __VA_ARGS__))
#define LOGV(_format_, ...)	(write_log(VERBOSE, LOG_TAG, _format_, ## __VA_ARGS__))

#define MAX_MESSAGE_SIZE_LOG 650
#define MAX_TAG_LENGTH 20

void
write_log(log_level level, char *tag, char *format, ...);
