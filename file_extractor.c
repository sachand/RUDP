#include "app_settings.h"

#define LOG_TAG "FileExtractor"

extern void
jitter_sleep ();

extern int xterm_pipe_fd[2];
extern boolean g_client_serve;

void *
do_remote_file_read (void *args)
{
    endpoint *p = (endpoint *)args;

    char *buf = (char *) malloc(FILE_BUFFER_READ_ONE_SHOT);
    if (FILE_BUFFER_READ_ONE_SHOT < MAX_PAYLOAD_SIZE)
    {
        LOGE("FILE_BUFFER_READ_ONE_SHOT set to %d", FILE_BUFFER_READ_ONE_SHOT);
        errno = EINVAL;
        return NULL;
    }

    uint16_t flags = 0;
    long total_bytes_read = 0;
    do
    {
        jitter_sleep();
        int received = recv_rudp(p, &flags, buf, FILE_BUFFER_READ_ONE_SHOT);
        if (0 < received)
        {
            total_bytes_read += received;
            LOGD("Read %d bytes from channel", received);
            LOGV("Write: %d", write(xterm_pipe_fd[1], buf, received));
            if (TRUE == is_flag_set(flags, MSG_HEADER_FLAG_FIM))
            {
                // Connection closed
                LOGS("File transfer complete");
                break;
            }
        }
        else
        {
            if (ETIMEDOUT == errno || EAGAIN == errno)
            {
                if (TRUE == g_client_serve)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
            LOGE("Error in connection while read: %s", errno_string());
            break;
        }
    } while (1);

    LOGS("Total Bytes received (by application): %ld", total_bytes_read);
    g_client_serve = FALSE;
    free(buf);
    close(xterm_pipe_fd[1]);
    return NULL;
}
