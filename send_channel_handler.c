#include "app_settings.h"

#define LOG_TAG "SendChannelHdlr"

void
print_send_channel_stats (send_channel *sc)
{
    if (NULL != sc)
    {
        LOGS("SEND WINDOW STATE: ");
    }
}

boolean
is_send_channel_locked (send_channel *sc)
{
    if (TRUE == is_queue_full(&(sc->window)))
    {
        return TRUE;
    }

    int smaller_wnd = sc->state.cwnd < sc->state.rwnd ? sc->state.cwnd : sc->state.rwnd;
    if (0 == smaller_wnd)
    {
        return TRUE;
    }

    int head_at_full = (sc->window.tail + smaller_wnd - 1) % sc->window.size;
    queue_element *last = &(sc->window.elements[head_at_full]);
    return (NULL == last->msg) ? FALSE : TRUE;
}

int
get_send_channel_capacity (channel *c)
{
    send_channel *sc = &(c->sink);
    int capacity = (sc->state.cwnd < sc->window.size) ? sc->state.cwnd : sc->window.size;
    LOGV("Real Capacity: %d", capacity);
    return (capacity - get_flight_size(sc)) >= 0 ? (capacity - get_flight_size(sc)) : 0;
}

uint32_t
transmit (SOCKET sock, queue_element *e)
{
    if (e->transmission_count > MAX_RETXMT)
    {
        // Oops! Destroy connection.
        print_sock_stats(sock, "Transmission count overflow.");
        errno = ETIMEDOUT;
        return SOCKET_ERROR;
    }

    uint32_t bytes_sent;
    bytes_sent = socket_send_msg_default(sock, e->msg);
    msg_header *h = e->msg[0].iov_base;

    e->timestamp = get_current_system_time_millis();
    e->transmission_count += 1;

    return bytes_sent;
}

typedef struct producer_params_t
{
    msg_iovec msg;
    channel *c;
} producer_params;

static void *
do_produce (void *args)
{
    producer_params *params = (producer_params *)args;
    channel *c = params->c;
    msg_iovec msg = params->msg;

    msg_header *header = msg[0].iov_base;
    char *payload = msg[1].iov_base;
    
    pthread_mutex_lock(&(c->sink.access_mutex));
        while (TRUE == is_send_channel_locked(&(c->sink)) &&
                FALSE == c->stopping)
        {
            LOGS("Window locked. Waiting for some space to free up");
            sem_post(&(c->sink.state.transmitted_something));
            pthread_cond_wait(&(c->sink.window_unlocked), &(c->sink.access_mutex));
        }

        if (TRUE == c->stopping)
        {
            pthread_mutex_unlock(&(c->sink.access_mutex));
            return NULL;
        }

        // Initialize header
        header->sequence_num = c->next_seq_num_send;
        c->next_seq_num_send += 1;
        header->advertized_window = get_empty_queue_count(&(c->source.window));
        header->payload_length = msg[1].iov_len;
        header->acknowledgment_num = c->last_seq_num_recv + 1;

        ///@todo return NULL??? mutex unlock???? xondition below?
        LOGV("Send channel capacity: %d", get_send_channel_capacity(c));
        int index = push(&(c->sink.window), msg);
        transmit(c->sock, &(c->sink.window.elements[index]));
        sem_post(&(c->sink.state.transmitted_something));
    pthread_mutex_unlock(&(c->sink.access_mutex));
    return args;
}

int
process_ack (channel *c, msg_header *header)
{
    int ret = 1;
    pthread_mutex_lock(&(c->sink.access_mutex));
        uint32_t msg_acked = header->acknowledgment_num;
        c->sink.state.rwnd = header->advertized_window;
        
        if (c->sink.state.rwnd > 0)
        {
            pthread_cond_signal(&(c->sink.window_unlocked));
        }

        if (TRUE == is_queue_empty(&(c->sink)))
        {
            pthread_mutex_unlock(&(c->sink.access_mutex));
            return 1;
        }

        queue_element *e_peeked = peek_tail(&(c->sink));
        msg_header *e_msg_header = e_peeked->msg[0].iov_base;
        
        queue_element e_popped;
        boolean popped = FALSE;
        
        while (FALSE == is_queue_empty(&(c->sink.window)) &&
                e_msg_header->sequence_num < msg_acked)
        {
            // Ack received for this. pop and do calcs
            popped = TRUE;
            pop(&(c->sink), &e_popped);
            pthread_cond_signal(&(c->sink.window_unlocked));
            if (1 == e_popped.transmission_count)
            {
                LOGV("Modifying RTXMT %ld %ld", get_current_system_time_millis(), e_popped.timestamp);
                rto_calculate(&(c->sink.state.retxmt_info),
                        (long)get_current_system_time_millis() - (long)e_popped.timestamp);
            }
            
            // Go for next element in queue and loop
            e_peeked = peek_tail(&(c->sink));
            if (NULL == e_peeked->msg)
            {
                break;
            }

            e_msg_header = e_peeked->msg[0].iov_base;
        }

        if (TRUE == popped)
        {
            // Signal retransmitter to restart.
            c->sink.duplicate_ack_count = 0;
            congestion_control_on_ack(&(c->sink));
            pthread_cond_signal(&(c->sink.state.stop_retransmission_alarm));
        }
        else if (FALSE == is_queue_empty(&(c->sink.window)))
        {
            // Means duplicate ACK
            c->sink.duplicate_ack_count += 1;
            LOGS("Duplicate ACK: %d. Dup Count: %d", msg_acked, c->sink.duplicate_ack_count);
            if (FAST_RETRANSMIT_ACK_COUNT == c->sink.duplicate_ack_count)
            {
                congestion_control_on_fast_retransmit(&(c->sink));
                pthread_cond_signal(&(c->sink.state.stop_retransmission_alarm));
                ret = transmit(c->sock, peek_tail(&(c->sink.window)));
            }
        }
        else
        {
            // Not duplicate but an out of order ACK.
            congestion_control_on_ack(&(c->sink));
        }
    pthread_mutex_unlock(&(c->sink.access_mutex));

    return ret;
}

/**
 * Blocking call that tries to deliver a message to the socket buffer.
 * This is multi-thread resilient. This means, multiple, clients can
 * try to send msgs to it in parallel.
 *
 * @param msg       Message to deliver
 *
 * @return If message sent successfully
 * */
uint32_t
send_msg (channel *c, msg_iovec msg)
{
    producer_params params;
    params.c = c;
    params.msg = msg;

    return (NULL == do_produce(&params)) ? SOCKET_ERROR : 1;
}
