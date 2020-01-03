#include "app_settings.h"

#define LOG_TAG "FileSender"

/**
 * Wraps fread.
 * 
 * This makes the impl platform independent but categorically inefficient.
 * Reason being standard doesn't specify what happens on partial reads.
 * Whether partial data is read or not is not guaranteed.
 * */
int
fread_wrapper(char *buf, FILE *f)
{
    int count = 0;
    for (; count < FILE_BUFFER_READ_ONE_SHOT; ++count, ++buf)
    {
        if (1 != fread(buf, 1, 1, f))
        {
            if (0 == feof(f))
            {
                LOGE("Error while reading file: %s", errno_string());
                count = -1;
            }
            break;
        }
    }

    return count;
}

/**
 * Transfers file over given endpoint
 * */
boolean
file_transfer (FILE *f, endpoint *p)
{
    if (0 >= FILE_BUFFER_READ_ONE_SHOT)
    {
        LOGE("FILE_BUFFER_SEND_ONE_SHOT set to %d", FILE_BUFFER_SEND_ONE_SHOT);
        errno = EINVAL;
        return FALSE;
    }
    
    char *buf = (char *) malloc(FILE_BUFFER_SEND_ONE_SHOT);
    if (NULL == buf)
    {
        LOGE("Buffer matching FILE_BUFFER_READ_ONE_SHOT cannot be created.");
        errno = ENOMEM;
        return FALSE;
    }
    
    uint32_t total_bytes_sent = 0;
    do
    {
        int elements_read = fread_wrapper(buf, f);
        if (-1 == elements_read)
        {
            break;
        }
        
        total_bytes_sent += elements_read;
        if ((int)FILE_BUFFER_READ_ONE_SHOT != elements_read)
        {
            if (0 != feof(f))
            {
                LOGI("Reached EOF");
                if (SOCKET_ERROR == send_rudp(p, MSG_HEADER_FLAG_FIM, buf,
                        elements_read))
                {
                    set_channel_stopping(p->sock);
                }
                else
                {
                    LOGD("Sent %d bytes to channel", elements_read);
                }
            }
            break;
        }
        else
        {
            if (SOCKET_ERROR == send_rudp(p, 0, buf, elements_read))
            {
                set_channel_stopping(p->sock);
                break;
            }
            else
            {
                LOGD("Sent %d bytes to channel", elements_read);
            }
        }
    } while (FALSE == is_channel_stopping(p->sock));

    LOGS("Total Bytes sent (from application): %u", total_bytes_sent);
    free(buf);
    return (0 == feof(f)) ? FALSE : TRUE;
}
