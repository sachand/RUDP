/**
 * Task of this component is to provide interface between underlying
 * protocol/channel and application using it.
 * */

#include <math.h>
#include "app_settings.h"

#define LOG_TAG "ChannelWrapper"

/**
 * Creates a message that our R-UDP will send. Whatever data is given
 * is buffered. So, caller may free its copy if it doesn't need it anymore.
 *
 * @param header            Message header
 * @param payload           Message payload
 * @param payload_length    Message payload length
 *
 * @return An iovec msg for the details given.
 * */
msg_iovec
create_msg (msg_header *header, char *payload, uint32_t payload_length)
{
    msg_iovec msg = (msg_iovec) calloc(2, sizeof(struct iovec));

    msg_header *header_copy = get_new_header();
    if (NULL != header)
    {
        memcpy(header_copy, header, sizeof(msg_header));
    }
    msg[0].iov_base = (char *)header_copy;
    msg[0].iov_len = HEADER_SIZE;
    
    char *payload_copy = NULL;
    if (NULL != payload)
    {
        payload_copy = (char *) calloc(payload_length, 1);
        memcpy(payload_copy, payload, payload_length);
    }

    msg[1].iov_base = (char *)payload_copy;
    msg[1].iov_len = payload_length;

    return msg;
}

/**
 * Creates an empty maximum sized message that this R-UDP supports
 * */
msg_iovec
create_msg_max ()
{
    msg_iovec msg = (msg_iovec) calloc(2, sizeof(struct iovec));

    msg_header *header_copy = get_new_header();    
    char *payload_copy = (char *) calloc(MAX_PAYLOAD_SIZE, 1);

    msg[0].iov_base = (char *)header_copy;
    msg[0].iov_len = HEADER_SIZE;

    msg[1].iov_base = (char *)payload_copy;
    msg[1].iov_len = MAX_PAYLOAD_SIZE;

    return msg;
}

/**
 * Frees given message. Deallocates heap memory.
 * 
 * @param msg       Message to destroy
 * */
void
delete_msg (msg_iovec msg)
{
    if (NULL != msg)
    {
        if (NULL != msg[0].iov_base)
        {
            free(msg[0].iov_base);
        }
        if (NULL != msg[1].iov_base)
        {
            free(msg[1].iov_base);
        }
        free(msg);
    }
}

/**
 * Checks if the size of buffer application wants to transmit is bigger
 * than the physical memory(not cwnd/rwnd) of our protocol
 * 
 * @param sink      The send channel
 * @param len       Length of buffer to send
 * */
boolean
is_payload_too_big (send_channel *sink, int len)
{
    uint32_t messages_needed = (uint32_t)ceil((double)len / (MAX_PAYLOAD_SIZE));
    return (messages_needed > sink->window.size) ? TRUE : FALSE;
}

/**
 * Converts given buffer from application to messages that can be passed
 * over theis R-UDP protocol.
 * 
 * @param p         Endpoint of the connection
 * @param flags     Flags to specify for this buffer. Ideally should be 0.
 * @param buf       Data to send
 * @param len       Length of data to send
 * 
 * @return Number of messages sent for given data. SOCKET_ERROR is returned
 *         if the connection saw some error. Check errno for error.
 * */
uint32_t
send_rudp (endpoint *p, uint16_t flags, char *buf, int len)
{
    // Sanity check
    if (NULL == p || NULL == buf || 0 >= len)
    {
        errno = EINVAL;
        return SOCKET_ERROR;
    }

    send_channel *sink = get_send_channel(p);
    if (TRUE == is_payload_too_big(sink, len))
    {
        errno = EMSGSIZE;
        return SOCKET_ERROR;
    }

    int bytes_to_send = len;
    uint32_t packets_sent = 1;    
    msg_iovec msg = NULL;
    msg_header *header = NULL;

    LOGV("Reading %d bytes from %p", len, buf);
    do
    {
        // Create a message and add in internal channel/window
        int msg_len = (bytes_to_send < MAX_PAYLOAD_SIZE) ?
                bytes_to_send : MAX_PAYLOAD_SIZE;

        LOGV("Creating %d bytes from %p", msg_len, buf);
        msg = create_msg(NULL, buf, msg_len);
        header = (msg_header *)msg[0].iov_base;
        header->flags |= flags;
        if (is_flag_set(flags, MSG_HEADER_FLAG_FIM))
        {
            header->flags |= MSG_HEADER_FLAG_ACK;
        }

        if (SOCKET_ERROR == send_msg(find_channel(p->sock), msg))
        {
            LOGE("Could not send message. Error: %s", errno_string());
            return SOCKET_ERROR;
        }

        ++packets_sent;
        bytes_to_send -= MAX_PAYLOAD_SIZE;
        buf += MAX_PAYLOAD_SIZE;
    } while (bytes_to_send > 0);

    return packets_sent;
}

/**
 * Reads a msg from the channel.
 * 
 * @param p         The endpoint to read from
 * @param msg_read  Handler on the data read. 
 * 
 * @return Number of messages read. SOCKET_ERROR in case of error and 0
 *         if the connection is being shutdown
 * 
 * NOTE:
 * This is currently not symmetric with send_msg which can send an arbitrary
 * char array. That should also be the case here.
 * */
int
recv_rudp (endpoint *p, uint16_t *flags, char *buf, int len)
{
    // Sanity checks
    if (NULL == p)
    {
        errno = EINVAL;
        return SOCKET_ERROR;
    }

    recv_channel *source = get_recv_channel(p);
    int bytes_read = 0;
    msg_iovec msg = NULL;
    
    do
    {
        if (FALSE == recv_msg(source, &msg))
        {
            //LOGV("Could not recv message. Error: %s", errno_string());
            return SOCKET_ERROR;
        }
        else
        {
            msg_header *header = msg[0].iov_base;
            if (header->payload_length > 0)
            {
                memcpy(buf, msg[1].iov_base, header->payload_length);
                buf += header->payload_length;
                bytes_read += header->payload_length;
            }
            if (TRUE == is_flag_set(header->flags, MSG_HEADER_FLAG_FIM))
            {
                *flags = header->flags;
                delete_msg(msg);
                break;
            }
            delete_msg(msg);
        }
    } while ((len - bytes_read) >= MAX_PAYLOAD_SIZE);

    return bytes_read;
}
