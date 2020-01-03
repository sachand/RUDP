#include "msg_header.h"

/**
 * Construct to point to the data part of a queue cell
 * */
typedef struct queue_element_t
{
    msg_iovec msg;
    uint64_t timestamp;
    uint8_t transmission_count;
} queue_element;

typedef queue_element *queue_element_list;

/**
 * Construct to represent a queue itself.
 * Implemented as an array simulating a queue.
 * */
typedef struct queue_t
{
    int size;
    int head;
    int tail;

    boolean initialized;

    queue_element_list elements;
} queue;
