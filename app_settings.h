#pragma once

#include "socket_common.h"
#include "endpoint.h"
#include "logger.h"
#include "extras.h"
#include "np_time.h"
#include "channel_provider.h"
#include "msg_header.h"
#include "socket_common.h"
#include "server_workers.h"
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>

/**
 * Buffer size for file name to be transferred
 * 
 * @todo use FILENAME_MAX instead
 * */
#define MAX_FILENAME 128

#define MAX_MESSAGE_SIZE 512
#define MAX_PAYLOAD_SIZE (MAX_MESSAGE_SIZE - HEADER_SIZE)

/**
 * Buffer size that application tries to read in one shot.
 * */
#define FILE_BUFFER_READ_ONE_SHOT MAX_PAYLOAD_SIZE
#define FILE_BUFFER_SEND_ONE_SHOT MAX_PAYLOAD_SIZE

/**
 * Server default port number.
 * Used if *.in not found
 * */
#define DEFAULT_PORT_NUMBER 47991

/**
 * Default initial rwnd and swnd sizes.
 * Used if *.in not found.
 * */
#define DEFAULT_WINDOW_SIZE_SEND 16
#define DEFAULT_WINDOW_SIZE_RECEIVE 16

/**
 * Invalid initials for msg_header
 * */
#define INVALID_PORT -1
#define INVALID_SEQUENCE_NUM -1
#define INVALID_ACKNOWLEDGMENT_NUM -1
#define INVALID_ADVERTIZED_WINDOW -1

/**
 * Protocol flags
 * */
#define MSG_HEADER_FLAG_FILENAME    (1 << 0)
#define MSG_HEADER_FLAG_SYN         (1 << 1)
#define MSG_HEADER_FLAG_ACK         (1 << 2)
#define MSG_HEADER_FLAG_FIM         (1 << 3)
#define MSG_HEADER_FLAG_PRB         (1 << 4)

/**
 * Heuristics for send/recv of control messages
 * */
#define INITIAL_CTL_SEND_TIMEOUT_MS 1000
#define INITIAL_CTL_READ_TIMEOUT_MS 1000
#define CTL_RETRY_COUNT 12

/**
 * Maximum Segment Lifetime = 30 seconds
 * RFC 793 suggests it is arbitrary and changeable per environment. We
 * assume 30 secs.
 * */
#define MSL_MS (30 * 1000)

/**
 * Maximum time for probe timeout.
 * 
 * RFC 1122 asks that an upper limit be kept but doesn't specify how much.
 * Arbitrarily choose 1 minute.
 * */
#define PROBE_MAX_TIMEOUT_MS (60 * 1000)

/**
 * Maximum number of seconds Xterm window will stay on after file transfer is
 * complete.
 * */
#define XTERM_SLEEP_AFTER_COMPLETE_S (60 * 60)

/**
 * Bounds for RTO
 * */
#define RTO_MIN_MS 1000
#define RTO_MAX_MS 3000

/**
 * Scale factors for SRTT and RTTVAR.
 * We store scaled values of SRTT and RTTVAR in order to have simpler
 * calculations.
 * 
 * Referring to RFC 6298, alpha = 2^(-1 * SRTT_SCALER) and
 * beta = 2^(-1 * RTTVAR_SCALER)
 * */
#define SRTT_SCALER 3
#define RTTVAR_SCALER 2

/**
 * Maximum number of retransmissions that retransmitter attempts
 * */
#define MAX_RETXMT 12

/**
 * Number of duplicate ACKs that need to accumulate in order for
 * Fast Retransmit, Fast Recovery to trigger
 * */
#define FAST_RETRANSMIT_ACK_COUNT 3
