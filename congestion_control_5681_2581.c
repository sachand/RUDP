#include "app_settings.h"

#define LOG_TAG "CongCont562581"

/**
 * Sets initial value of congestion window.
 * Conforms to RFC 5681's max limits.
 * */
void
__init_cwnd (send_channel *c)
{
    send_channel_state *scs = &(c->state);
    scs->cwnd = 1;
}

/**
 * Initializes ssthresh.
 * The assignment conforms to RFC 5681.
 * */
void
__init_ssthresh (send_channel *c)
{
    send_channel_state *scs = &(c->state);
    scs->ssthresh = c->window.size;
}

/**
 * Initilializes congestion state of given channel
 * */
void
init_congestion_state(send_channel *c)
{
    __init_cwnd(c);
    __init_ssthresh(c);
    c->state.ack_per_round_trip = 0;
}

/**
 * Number of un-acked messages
 * */
int
get_flight_size (send_channel *c)
{
    return c->window.size - get_empty_queue_count(&(c->window));
}

/**
 * Calculates new ssthresh value conforming to RFC standard:
 * ssthresh <= max (FlightSize / 2, 2*SMSS).
 * 
 * We are optimistically setting to the highest possible.
 * */
int
__new_ssthresh (send_channel *c)
{
    int flight_size_2 = get_flight_size(c) >> 1;
    return (flight_size_2 > 2) ? flight_size_2 : 2;
}

/**
 * Easy implementation for cwnd. This is not conforming to RFC 5681 or even 2001.
 * We don't inflate the window.
 * 
 * @todo Implement if time permits
 * */
void
congestion_control_on_fast_retransmit (send_channel *c)
{
    send_channel_state *scs = &(c->state);
    LOGV("Old cwnd: %d ssthresh: %d", scs->cwnd, scs->ssthresh);
    scs->ssthresh = __new_ssthresh(c);
    scs->cwnd = scs->ssthresh;
    LOGS("Updated cwnd: %d ssthresh: %d Reason: Fast Recovery", scs->cwnd, scs->ssthresh);
}

/**
 * When a TCP sender detects segment loss using the retransmission timer
 * and the given segment has not yet been resent by way of the
 * retransmission timer, the value of ssthresh MUST be set to no more
 * than the value given in the equation:
 *
 *    ssthresh = max (FlightSize / 2, 2*SMSS)
 *
 * where, as discussed above, FlightSize is the amount of outstanding
 * data in the network.
 *
 * On the other hand, when a TCP sender detects segment loss using the
 * retransmission timer and the given segment has already been
 * retransmitted by way of the retransmission timer at least once, the
 * value of ssthresh is held constant.
 * 
 * For us SMSS is equivalent to 1.
 * */
void
congestion_control_on_transmit_timeout (send_channel *c)
{
    send_channel_state *scs = &(c->state);
    LOGV("Old cwnd: %d ssthresh: %d", scs->cwnd, scs->ssthresh);
    scs->ssthresh = __new_ssthresh(c);
    scs->cwnd = 1;
    LOGS("Updated cwnd: %d ssthresh: %d Reason: Transmit Timeout", scs->cwnd, scs->ssthresh);
}

/**
 * Slow start algorithm will be used when cwnd < ssthresh
 * Additive Inverse is used otherwise.
 * 
 * This additionally ensures cwnd never crosses window size * 2. The
 * reason for restriction is because ssthresh does not depend on cwnd. It
 * depends on the flight size which will be the lower of cwnd and
 * rwnd. And as the window size is set to rwnd, if cwnd is grown
 * wihtout control, it will have no impact as rwnd will dominate the
 * calculations and window operations. Also, under Fast recovery and
 * retransmission timeout, cwnd is calculated on the basis of ssthresh
 * and not the old value of cwnd.
 * 
 * The factor 2 is chosen because ssthresh cannot exceed FlightSize * 2
 * which can be at max (intial rwnd) * 2.
 * 
 * And in case of no congestion ever condition, cwnd can theoretically
 * overflow. This prevents that from happening.
 * */
void
congestion_control_on_ack (send_channel *c)
{
    send_channel_state *scs = &(c->state);
    queue *q = &(c->window);

    LOGV("Old cwnd: %d ssthresh: %d", scs->cwnd, scs->ssthresh);
    if (scs->cwnd < scs->ssthresh)
    {
        // Increase window size by 1
        if ((scs->cwnd >> 1) < q->size)
        {
            scs->cwnd += 1;
        }
        LOGS("Updated cwnd: %d ssthresh: %d State before update: SS", scs->cwnd, scs->ssthresh);
    }
    else
    {
        scs->ack_per_round_trip += 1;
        if (scs->ack_per_round_trip >= scs->cwnd)
        {
            // Increase window size by 1/n * n
            if ((scs->cwnd >> 1) < q->size)
            {
                scs->cwnd += 1;
            }
            scs->ack_per_round_trip = 0;
        }
        LOGS("Updated cwnd: %d ssthresh: %d State before update: AI", scs->cwnd, scs->ssthresh);
    }
}
