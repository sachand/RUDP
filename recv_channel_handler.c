#include "app_settings.h"

#define LOG_TAG "RecvChannelHdlr"

#define MAX_RECV_WAIT_MS ((long)(RTO_MAX_MS * ((1 << (MAX_RETXMT + 1)) - 1)))

static long last_contact = 0;

boolean
is_recv_channel_locked (recv_channel *c)
{
    return (-1 == first_hole(&(c->window))) ? TRUE : FALSE;
}

static void *
do_produce (channel *c)
{
    recv_channel *rc = &(c->source);
    msg_iovec msg = NULL;

    if (0 >= socket_recv_msg_default(c->sock, &msg, 5000))
    {
        if (((long)get_current_system_time_millis() - last_contact) > MAX_RECV_WAIT_MS)
        {
            LOGS("Did not receive anything on the socket for %ld seconds. Quitting",
                    (MAX_RECV_WAIT_MS / 1000));
            c->stopping = TRUE;
            errno = ETIMEDOUT;
        }
        return NULL;
    }

    last_contact = (long)get_current_system_time_millis();
    msg_header *header = msg[0].iov_base;
    char *payload = msg[1].iov_base;
    uint16_t flags = header->flags;
    uint32_t this_seq_num;
    endpoint p;
    p.sock = c->sock;

    if (0 == header->payload_length)
    {
        if (MSG_HEADER_FLAG_ACK == header->flags)
        {
            process_ack(c, header);
        }
        else if ((MSG_HEADER_FLAG_FIM | MSG_HEADER_FLAG_ACK) == flags)
        {
            LOGS("Got _FIM_ACK_ from other party.");
            errno = 0;
            delete_msg(msg);
            return NULL;
        }
        else if (MSG_HEADER_FLAG_PRB == header->flags)
        {
            send_rudp_ctl_int(&p, MSG_HEADER_FLAG_ACK, NULL, 0, 0, 0);
        }
        delete_msg(msg);
    }
    else
    {
        // Add in recv_window
        pthread_mutex_lock(&(rc->access_mutex));
            int index = -1;
            this_seq_num = header->sequence_num;
            LOGV("First hole in window: %d", first_hole(&(c->source.window)));
            LOGV("Last recv seq num: %d", c->last_seq_num_recv);
            if (TRUE == is_recv_channel_locked(rc))
            {
                LOGS("Window locked. Discarding received data");
            }
            else
            {
                int prev_head = c->source.window.head;
                if ((MSG_HEADER_FLAG_FIM | MSG_HEADER_FLAG_ACK) == header->flags &&
                        c->last_seq_num_recv != header->sequence_num - 1)
                {
                    index = -1;
                    LOGS("Discarding out of order FIM_ACK");
                }
                else
                {
                    index = push_at(&(c->source.window), msg, c->last_seq_num_recv);
                    if (-1 == index)
                    {
                        LOGS("Too advanced or too old buffer arrived. Discarding");
                    }
                }

            }
            
            if (-1 == index)
            {
                // Could not enter what was read. Free it
                delete_msg(msg);
            }
            else
            {
                // Data entered.
                int first_hole_in_window = first_hole(&(c->source.window));
                if (-1 == first_hole_in_window)
                {
                    index = (c->source.window.tail - 1 + c->source.window.size) % c->source.window.size;
                }
                else
                {
                    index = c->source.window.tail;
                    if (first_hole_in_window != c->source.window.tail)
                    {
                        index = (first_hole_in_window - 1 + c->source.window.size) % c->source.window.size;
                    }
                }
                LOGV("Index: %d", index);
                msg_header *hdr = c->source.window.elements[index].msg[0].iov_base;
                c->last_seq_num_recv = hdr-> sequence_num;
                if (index == c->source.window.tail && TRUE == is_hole(&(c->source.window), index))
                {
                    c->last_seq_num_recv -= 1;
                }
            }

            flags = MSG_HEADER_FLAG_ACK;
            if (-1 != index &&
                    (MSG_HEADER_FLAG_FIM | MSG_HEADER_FLAG_ACK) == header->flags &&
                    header->sequence_num == c->last_seq_num_recv)
            {
                //Packet successfully stored inside window
                flags |= MSG_HEADER_FLAG_FIM;
            }
            send_rudp_ctl_int(&p, flags, NULL, 0, 0, 0);
        pthread_mutex_unlock(&(rc->access_mutex));
    }

    if ((MSG_HEADER_FLAG_FIM | MSG_HEADER_FLAG_ACK) == flags &&
            this_seq_num == c->last_seq_num_recv)
    {
        LOGS("Got _FIM_ACK_ from other party.");
        errno = 0;
        return NULL;
    }

    return c;
}

boolean
should_continue ()
{
    return (ETIMEDOUT   == errno ||
            EAGAIN      == errno ||
            EWOULDBLOCK == errno) ? TRUE : FALSE;
}

void *
producer (void *args)
{
    LOGS("Socket Reader started");
    channel *c = (channel *)args;
    last_contact = get_current_system_time_millis();
    do
    {
        if (NULL == do_produce(c) && FALSE == should_continue())
        {
            break;
        }
    } while (FALSE == c->stopping);
    
    c->stopping = TRUE;
    pthread_cond_signal(&(c->sink.window_unlocked));
    LOGI("Channel receiver dying. Error: %s", errno_string());
    return NULL;
}

/**
 * NON-Blocking call that tries to read a message.
 * This is multi-thread resilient. This means, multiple, clients can
 * try to read msgs from it in parallel.
 *
 * @param msg       Message read
 *
 * @return If message received successfully
 * */
boolean
recv_msg (recv_channel *source, msg_iovec *msg)
{
    boolean ret = TRUE;
    pthread_mutex_lock(&(source->access_mutex));
        if (TRUE == is_queue_empty(&(source->window)))
        {
            errno = EAGAIN;
            ret = FALSE;
        }
        else
        {
            ret = pop_msg(&(source->window), msg);
        }
    pthread_mutex_unlock(&(source->access_mutex));

    return ret;
}

pthread_t
recv_channel_start (endpoint *p)
{
    LOGV("Creating SocketReader");
    pthread_t socket_reader;
    pthread_create(&socket_reader, NULL, &producer, find_channel(p->sock));
    return socket_reader;
}
