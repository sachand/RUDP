#include "app_settings.h"

#define LOG_TAG "Rtxmt6298"

void
__rto_calculate(retransmission_state *s)
{
    s->rto_ms = s->rttvar_scaled + (s->srtt_scaled >> SRTT_SCALER);
    if (s->rto_ms < RTO_MIN_MS)
    {
        s->rto_ms = RTO_MIN_MS;
    }
    else if (s->rto_ms > RTO_MAX_MS)
    {
        s->rto_ms = RTO_MAX_MS;
    }
}

void
rto_calculate (retransmission_state *s, long recent_rtt)
{
    pthread_mutex_lock(&s->synchronizer);
        long real_recent_rtt = recent_rtt;
        LOGV("Initial RTO: %d Scaled SRTT: %d Scaled RTTVAR: ", s->rto_ms,
                s->srtt_scaled, s->rttvar_scaled);
        recent_rtt -= (s->srtt_scaled >> SRTT_SCALER);
        s->srtt_scaled += recent_rtt;

        if (recent_rtt < 0)
        {
            recent_rtt = -recent_rtt;
        }

        recent_rtt -= (s->rttvar_scaled >> RTTVAR_SCALER);
        s->rttvar_scaled += recent_rtt;
        
        __rto_calculate(s);
        LOGS("Updated RTO: %d Scaled SRTT: %d Scaled RTTVAR: %d Current RTT: %ld",
                s->rto_ms, s->srtt_scaled, s->rttvar_scaled, real_recent_rtt);
    pthread_mutex_unlock(&s->synchronizer);
}

void
init_retransmission_state (retransmission_state *s)
{
    s->srtt_scaled = 0;
    s->rttvar_scaled = 750 << RTTVAR_SCALER;
    __rto_calculate(s);
    pthread_mutex_init(&(s->synchronizer), NULL);
}

void
reset_retransmission_state (retransmission_state *s)
{
    s->rto_ms = 0;
    s->srtt_scaled = 0;
    s->rttvar_scaled = 0;
}

void
destroy_retransmission_state (retransmission_state *s)
{
    pthread_mutex_destroy(&s->synchronizer);
}

