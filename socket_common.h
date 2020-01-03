#pragma once

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>

typedef int SOCKET;

#define SOCKET_ERROR (-1)

#define WATCH_CALL(_call_, _msg_)                       \
    do                                                  \
    {                                                   \
        if (SOCKET_ERROR == _call_)                     \
        {                                               \
            if (EINTR != errno)                         \
            {                                           \
                LOGWTF("%s failed with error: %s",      \
                        _msg_, errno_string());         \
            }                                           \
            else                                        \
            {                                           \
                LOGE("%s interrupted", _msg_);          \
            }                                           \
            return;                                     \
        }                                               \
    } while (0);                                        \


#define EXEC_BARE(_x_, _y_)                             \
    do                                                  \
    {                                                   \
        if (SOCKET_ERROR == _x_)                        \
        {                                               \
            LOGE("%s Error: %s", _y_,                   \
                    errno_string());                    \
            return;                                     \
        }                                               \
    } while (0);                                        \


#define EXEC_CTL(_x_, _y_)                              \
    do                                                  \
    {                                                   \
        if (SOCKET_ERROR == _x_)                        \
        {                                               \
            LOGE("%s Error: %s", _y_,                   \
                    errno_string());                    \
            return FALSE;                               \
        }                                               \
    } while (0);                                        \

