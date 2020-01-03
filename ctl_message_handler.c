#include "app_settings.h"

#define LOG_TAG "CtlMsgHandler"

static uint32_t g_seq_num_hack = INVALID_SEQUENCE_NUM;

channel *
syn_preprocess (SOCKET sock, uint32_t window_size)
{
    // Init the params for channel components but dont really create the windows.
    channel *c = find_channel(sock);

    // Create recv window
    init_recv_channel(&(c->source), window_size);

    print_channel_stats(c);
    return c;
}

void
syn_postprocess (SOCKET sock, msg_header *header)
{
    // Init the params for channel components but dont really create the windows.
    channel *c = find_channel(sock);

    // Synchronize sequence number for recv channel.
    c->last_seq_num_recv = header->sequence_num;

    // Create and init the sink or send channel.
    init_send_channel(c, header->advertized_window);
    
    print_channel_stats(c);
}

/**
 * Sends a control message across channel.
 * Control messages aren't kept in window otherwise we'll end up say
 * ACKing for ACK's. Thus, the design is to handle them separately.
 * 
 * @todo Try and integrate with default channel for a better design.
 * 
 * @param p         Endpoint to which ctl message has to be sent
 * @param flags     Flags to add in the ctl message
 * @param buf       Any data has to be sent along like filename
 * @param len       Size of buf
 * 
 * @return On success, it returns how manny ctl messages were sent.
 * On failure, SOCKET_ERROR is returned with errno set.
 * */
uint32_t
send_rudp_ctl_int (endpoint *p, uint16_t flags, char *buf, uint32_t len, uint32_t retxmt, uint32_t timeout_ms)
{
    msg_header *header = NULL;
    msg_iovec msg = NULL;
    uint32_t recv_window_size = 0;
    channel *c = NULL;

    if (0 > p->sock)
    {
        return SOCKET_ERROR;
    }

    switch(flags)
    {
    case MSG_HEADER_FLAG_FILENAME:
        if (NULL == buf)
        {
            errno = EINVAL;
            return SOCKET_ERROR;
        }
        msg = create_msg(NULL, buf, len);
        break;
        
    case MSG_HEADER_FLAG_SYN:
        if (0 == memcmp(&(p->network_address.sin_port), buf, len))
        {
            c = syn_preprocess(p->sock, DEFAULT_WINDOW_SIZE_RECEIVE);
            g_seq_num_hack = c->next_seq_num_send;
        }
        msg = create_msg(NULL, buf, len);
        header = (msg_header *)(msg[0].iov_base);
        header->sequence_num = g_seq_num_hack;
        header->advertized_window = DEFAULT_WINDOW_SIZE_RECEIVE;
        break;
        
    case (MSG_HEADER_FLAG_SYN | MSG_HEADER_FLAG_ACK):
        if (NULL == buf)
        {
            errno = EINVAL;
            return SOCKET_ERROR;
        }
        c = syn_preprocess(p->sock, *(uint32_t*)buf);
        msg = create_msg(NULL, NULL, 0);
        header = (msg_header *)(msg[0].iov_base);
        header->sequence_num = c->next_seq_num_send;
        header->acknowledgment_num = c->last_seq_num_recv + 1;
        header->advertized_window = *(uint32_t*)buf;
        len = 0;
        break;

    case MSG_HEADER_FLAG_ACK:
        retxmt = 0;
        timeout_ms = 0;
    case (MSG_HEADER_FLAG_FIM | MSG_HEADER_FLAG_ACK):
        c = find_channel(p->sock);
        msg = create_msg(NULL, buf, len);
        header = (msg_header *)(msg[0].iov_base);
        header->sequence_num = c->next_seq_num_send;
        header->acknowledgment_num = c->last_seq_num_recv + 1;
        header->advertized_window = get_empty_queue_count(&(c->source.window));
        break;

    case MSG_HEADER_FLAG_PRB:
        msg = create_msg(NULL, NULL, 0);
        break;
    }

    if (NULL == msg)
    {
        errno = EPROTO;
        LOGE("Unhandleable state transition requested %s", msg_header_flags_to_string(flags));
        return SOCKET_ERROR;
    }
    
    header = (msg_header *)(msg[0].iov_base);
    header->flags = flags;
    header->payload_length = len;

    int bytes_sent = SOCKET_ERROR;
    int transmission_count = -1;
    do
    {
        bytes_sent = socket_send_msg_default(p->sock, msg);
        LOGV("Sent %d", bytes_sent);
        if (SOCKET_ERROR == bytes_sent)
        {
            LOGE("Socket send failed. Error: %s", errno_string());
            bytes_sent = SOCKET_ERROR;
            break;
        }

        ++transmission_count;
        int rsc = ready_socket_count(p->sock, timeout_ms);
        if (0 < rsc)
        {
            // something arrived
            bytes_sent = 1;
            break;
        }
        else if (0 == rsc)
        {
            // timedout
            LOGV("Send count: %d", transmission_count);
            if (transmission_count >= (int)retxmt)
            {
                if (0 != retxmt)
                {
                    LOGS("Did not receive anything from other end after %d tries. Quitting",
                            transmission_count + 1);
                }
                errno = ETIMEDOUT;
                bytes_sent = SOCKET_ERROR;
                break;
            }
            else
            {
                LOGS("Did not receive anything from other end after %d tries.",
                        transmission_count + 1);
                timeout_ms <<= 1;
            }
        }
        else
        {
            LOGE("Socket select failed. Error: %s", errno_string());
            bytes_sent = SOCKET_ERROR;
            break;
        }
    } while (1);

    delete_msg(msg);
    return bytes_sent;
}

uint32_t
send_rudp_ctl (endpoint *p, uint16_t flags, char *buf, uint32_t len)
{
    return send_rudp_ctl_int(p, flags, buf, len, CTL_RETRY_COUNT, INITIAL_CTL_SEND_TIMEOUT_MS);
}

/**
 * Reads a control message from channel.
 * Control messages aren't kept in window otherwise we'll end up say
 * ACKing for ACK's. Thus, the design is to handle them separately.
 * 
 * @todo Try and integrate with default channel for a better design.
 * 
 * @param p         Endpoint from which ctl message has to be read
 * @param flags     Flags to expect in the ctl message
 * @param buf       Any data has to be read from the msg like filename
 * @param len       Size of buf
 * 
 * @return On success, it returns how manny ctl messages were read.
 * On failure, SOCKET_ERROR is returned with errno set. If this call
 * returns 0, this means it received a FIN message.
 * */
uint32_t
recv_rudp_ctl_ints (SOCKET sock, uint16_t *flags, char *buf, uint32_t len, uint32_t retxmt, uint32_t timeout_ms)
{
    msg_iovec msg_read;
    msg_header *header;

    int bytes_received = SOCKET_ERROR;
    int retry_count = -1;
    do
    {
        LOGV("Trying to read");
        bytes_received = socket_recv_msg_default(sock, &msg_read, timeout_ms);
        ++retry_count;
        if (0 < bytes_received)
        {
            // something arrived
            header = (msg_header *)msg_read[0].iov_base;
            
            if (TRUE == is_flag_set(header->flags, MSG_HEADER_FLAG_FIM))
            {
                *flags = header->flags;
                delete_msg(msg_read);
                return 0;
            }
            if (NULL == flags)
            {
                break;
            }
            if (*flags == header->flags)
            {
                LOGV("Headers matched %0x", header->flags);
                break;
            }
            else
            {
                ///@todo Add extra logic here
                LOGV("Header mismatch %0x %0x", *flags, header->flags);
                delete_msg(msg_read);
                return SOCKET_ERROR;
            }
        }
        else if (0 == bytes_received)
        {
            // timedout
            if (retry_count == retxmt)
            {
                if (0 != retxmt)
                {
                    LOGS("Did not receive anything from other end after %d tries. Quitting",
                            retry_count + 1);
                }
                errno = ETIMEDOUT;
                return SOCKET_ERROR;
            }
            else
            {
                LOGS("Did not receive anything from other end after %d tries.",
                        retry_count + 1);
                timeout_ms <<= 1;
            }
        }
        else
        {
            return SOCKET_ERROR;
        }
    } while (1);

    if (NULL != flags)
    {
        *flags = header->flags;
    }
    uint32_t payload_size = header->payload_length;

    channel *c = find_channel(sock);
    switch(header->flags)
    {
    case MSG_HEADER_FLAG_FILENAME:
        if (NULL != buf)
        {
            strncpy(buf, (char *)(msg_read[1].iov_base), payload_size);
            buf[payload_size] = '\0';
            LOGS("Filename received from client: %s", buf);
        }
        delete_msg(msg_read);
        return 1;
        
    case MSG_HEADER_FLAG_SYN:
        syn_postprocess(sock, header);
        strncpy(buf, (char *)(msg_read[1].iov_base), len);
        delete_msg(msg_read);
        return 1;
        
    case (MSG_HEADER_FLAG_SYN | MSG_HEADER_FLAG_ACK):
        syn_postprocess(sock, header);
        c->next_seq_num_send += 1;
        delete_msg(msg_read);
        return 1;

    case MSG_HEADER_FLAG_ACK:
        c->last_seq_num_recv = header->sequence_num;
        delete_msg(msg_read);
        return 1;
    }

    errno = EPROTO;
    LOGE("Unhandleable state transition requested by peer: %s",
            msg_header_flags_to_string(*flags));
    return SOCKET_ERROR;
}

uint32_t
recv_rudp_ctl_int (endpoint *p, uint16_t *flags, char *buf, uint32_t len, uint32_t retxmt, uint32_t timeout_ms)
{
    return recv_rudp_ctl_ints(p->sock, flags, buf, len, CTL_RETRY_COUNT, INITIAL_CTL_READ_TIMEOUT_MS);
}

uint32_t
recv_rudp_ctl (endpoint *p, uint16_t *flags, char *buf, uint32_t len)
{
    return recv_rudp_ctl_int(p, flags, buf, len, CTL_RETRY_COUNT, INITIAL_CTL_READ_TIMEOUT_MS);
}

int
peek_remove_rudp_ctl_one_shot(SOCKET sock, uint16_t discard_flag, uint16_t accept_flag, char *buf, uint32_t len)
{
    msg_iovec msg = NULL;
    int ret = socket_peek_msg_default(sock, &msg);

    if (ret > 0)
    {
        msg_header *h = msg[0].iov_base;
        if (h->flags == discard_flag)
        {
            socket_recv_msg_default(sock, &msg, 0);
            ret = discard_flag;
        }
        else if (h->flags == accept_flag)
        {
            ret = accept_flag;
        }
    }
    else
    {
        ret = SOCKET_ERROR;
    }

    delete_msg(msg);
    return ret;
}
