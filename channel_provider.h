/**
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                          CORE CHANNEL
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * */

#pragma once

#include "queue.h"
#include "retransmission_timer_6298.h"
#include <semaphore.h>

/**
 * Encapsulates information about the send_channel
 * */
typedef struct send_channel_state_t
{
    //-----------------------------------------------------------------
    // Congestion control
    ///@todo May be give congestion control its own struct...
    /**
     * CONGESTION WINDOW (cwnd): A TCP state variable that limits the amount
     * of data a TCP can send.  At any given time, a TCP MUST NOT send
     * data with a sequence number higher than the sum of the highest
     * acknowledged sequence number and the minimum of cwnd and rwnd.
     * */
    int cwnd;

    /**
     * SLOW START THRESHOLD (ssthresh): The slow start threshold is used
     * to determine whether the slow start or congestion avoidance algorithm
     * is used to control data transmission.
     * */
    int ssthresh;

    /**
     * ACKNOWLEDGMENTS PER RTT (ack_per_round_trip): This is the number of
     * acks that we recieved per RTT. This is only used in congestion
     * avoidance or AI mode.
     * */
    int ack_per_round_trip;

    //-----------------------------------------------------------------
    // Retransmission control
    /**
     * State to represent current retransmission engine
     * */
    retransmission_state retxmt_info;
    
    /**
     * retransmitter is the thread that actually retransmits data.
     * */
    pthread_t retransmitter;
    
    /**
     * stop_retransmission_alarm is the condition that retransmitter waits on.
     * When a message is transmitted, retransmitter, waits on this
     * condition for rto timeout. If meanwhile, this condition is signalled,
     * retransmitter is signalled which makes it break out of its sleep.
     * */
    pthread_cond_t stop_retransmission_alarm;

    /**
     * transmitted_something essentially signals the retransmitter that
     * something was just transmitted, so maybe retransmitter should watch
     * that.
     * */
    sem_t transmitted_something;

    /**
     * Dynamically changing size of the receive window of other party
     * */
    int rwnd;
} send_channel_state;

/**
 * Encapsulates the send channel itself
 * */
typedef struct send_channel_t
{
    /**
     * The circular queue that stores data
     * */
    queue window;

    /**
     * State information of the send channel
     * */
    send_channel_state state;

    /**
     * We want restricted access to this channel.
     * access_mutex to guard the channel.
     * 
     * NOTE: Through this mutex, we guard the window.
     * window on itself is unguarded.
     * */
    pthread_mutex_t access_mutex;

    /**
     * Signal that tells if window was just unlocked
     * */
    pthread_cond_t window_unlocked;

    /**
     * Stores duplicate ack count for the oldest member in the window
     * */
    int duplicate_ack_count;

    /**
     * Whether the channel is active or not. Inactive channels are either
     * babies(un-initialized ones) or oldies(dying ones).
     * */
    boolean active;
} send_channel;

/**
 * Encapsulates recv channel itself
 * */
typedef struct recv_channel_t
{
    /**
     * The circular queue that stores data
     * */
    queue window;

    /**
     * We want restricted access to this channel.
     * access_mutex to guard the channel.
     * 
     * NOTE: Through this mutex, we guard the window.
     * window on itself is unguarded.
     * */
    pthread_mutex_t access_mutex;

    /**
     * Whether the channel is active or not. Inactive channels are either
     * babies(un-initialized ones) or oldies(dying ones).
     * */
    boolean active;
} recv_channel;

/**
 * Channel id.
 * Each channel is recognized by its endpoints pair.
 * */
typedef struct channel_id_t
{
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
} channel_id;

/**
 * The channel.
 * Each channel has two parts - the send channel and the recv channel.
 * */
typedef struct channel_t
{
    channel_id id;
    send_channel sink;
    recv_channel source;
    SOCKET sock;
    uint32_t next_seq_num_send;
    uint32_t last_seq_num_recv;
    boolean stopping;

    struct channel_t *next;
} channel;

typedef channel *channel_list;

/**
 * Gives channel id for given socket
 * */
channel_id
channel_id_from_socket (SOCKET sock);
