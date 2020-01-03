#pragma once

#include <pthread.h>

/**
 * Holder of necessary retransmission info
 * */
typedef struct retransmission_state_t
{
    /**
     * Current RTO
     * */
    int rto_ms;
    
    /**
     * Scaled up value of Smoothed RTT
     * */
    int srtt_scaled;

    /**
     * Scaled up value of RTT variance
     * */
    int rttvar_scaled;

    pthread_mutex_t synchronizer;
}retransmission_state;

void
rto_calculate (retransmission_state *s, long recent_rtt);

void
rto_timeout (retransmission_state *s);

void
init_retransmission_state (retransmission_state *s);

void
destroy_retransmission_state (retransmission_state *s);

void
reset_retransmission_state (retransmission_state *s);
