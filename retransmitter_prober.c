#include "app_settings.h"

#define LOG_TAG "RetxmtProbe"

extern uint32_t
transmit (SOCKET sock, queue_element *e);

extern boolean
is_send_channel_locked (send_channel *sc);

static int probe_count = 0;
static int probe_timeout = 0;

static void
cleanup (void *args)
{
    channel *c = (channel *)args;
    c->stopping = TRUE;
    pthread_mutex_unlock(&(c->sink.access_mutex));
}

int
__retransmission_wait (channel *c, int timeout_ms)
{
    send_channel *s = &(c->sink);
    struct timespec abs_timeout = get_abstime_after(timeout_ms);

    LOGV("Starting timer %d", timeout_ms);
    return pthread_cond_timedwait(&(s->state.stop_retransmission_alarm),
            &(s->access_mutex), &abs_timeout);
}

int
__probe_round (channel *c)
{
    send_channel *s = &(c->sink);
    
    // Send probe message
    endpoint p;
    p.sock = c->sock;
    send_rudp_ctl_int(&p, MSG_HEADER_FLAG_PRB, NULL, 0, 0, 0);
    
    // Set timeout
    probe_timeout = s->state.retxmt_info.rto_ms << probe_count;
    ++probe_count;

    if (probe_timeout > PROBE_MAX_TIMEOUT_MS)
    {
        probe_timeout = PROBE_MAX_TIMEOUT_MS;
    }

    // Wait
    int res = __retransmission_wait(c, probe_timeout);
    if (0 != res)
    {
        // Condition didn't meet
        if (ETIMEDOUT != res)
        {
            LOGE("Something wrong with timedwait. Retransmitter trylock failed. Error: %s",
                    errno_to_string(res));
            return -1;
        }
    }
    else
    {
        // This thread actually locked retransmission_alarm.
        // Everything is cool.
        LOGV("Return value of timedwait: %d", res);
        return 0;
    }

    sem_post(&(s->state.transmitted_something));
    return 0;
}

static int
get_wnd_size (send_channel *s)
{
    return (s->state.cwnd < s->state.rwnd) ? s->state.cwnd : s->state.rwnd;
}

static int
__retransmit_wnd (channel *c)
{
    send_channel *s = &(c->sink);

    int i, j;
    int wnd_size = get_wnd_size(s);
    int send_size = (wnd_size > 1) ? wnd_size : 1;
    for (i = 0; i < send_size; ++i)
    {
        j = (s->window.tail + i) % s->window.size;
        if (SOCKET_ERROR == transmit(c->sock, &(s->window.elements[j])))
        {
            return SOCKET_ERROR;
        }
    }
    return 1;
}

void *
do_retransmit (void *args)
{
    channel *c = (channel *)args;
    send_channel *s = &(c->sink);
    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    pthread_cleanup_push(&cleanup, c);
    do
    {
        sem_wait(&(s->state.transmitted_something));
        LOGV("Retransmitter fired");
                
        pthread_mutex_lock(&(s->access_mutex));
            if (FALSE == is_queue_empty(&(s->window)))
            {
                queue_element *elem_tail = peek_tail(&(s->window));
                int retxmt_count = (int)elem_tail->transmission_count - 1;
                int res = __retransmission_wait(c, (s->state.retxmt_info.rto_ms << retxmt_count));
                probe_count = 0;
                
                if (0 != res)
                {
                    // Condition didn't meet!
                    if (ETIMEDOUT == res)
                    {
                        LOGS("Timedout. Retxmtting %u. Retxmt count: %d",
                                get_queue_element_seq_num(&(s->window), s->window.tail),
                                s->window.elements[s->window.tail].transmission_count);
                        // The lock timed out. This means ACK did not come.
                        // Time to retransmit...
                        congestion_control_on_transmit_timeout(s);
                        if (SOCKET_ERROR == transmit(c->sock, peek_tail(&(s->window))))
                        {
                            LOGE("Cannot write on socket. Error: %s", errno_string());
                            if (EAGAIN != errno && EWOULDBLOCK != errno)
                            {
                                break;
                            }
                        }
                        sem_post(&(s->state.transmitted_something));
                    }
                    else
                    {
                        LOGE("Something wrong with timedwait. Retransmitter trylock failed. Error: %s",
                                errno_to_string(res));
                        break;
                    }
                }
                else
                {
                    // This thread actually locked retransmission_alarm.
                    // Everything is cool.
                    LOGV("Return value of timedwait: %d", res);
                }
            }
            else
            {
                // Queue empty
                if (0 == s->state.rwnd)
                {
                    if (-1 ==__probe_round(c))
                    {
                        break;
                    }
                }
            }
        pthread_mutex_unlock(&(s->access_mutex));
        pthread_testcancel();
    } while (1);
    pthread_cleanup_pop(1);

    return NULL;
}
