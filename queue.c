/**
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *          NOT TO BE USED BY ANYONE EXCEPT queue_manager
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * */

#include "app_settings.h"

#define LOG_TAG "Queue"

void
print_queue_stats (queue *w)
{
    if (NULL != w)
    {
        LOGV("Queue stats: Head=%d Tail=%d", w->head, w->tail);
    }
}

void
destroy_queue_element (queue_element *item)
{
    delete_msg(item->msg);
}

void
destroy_queue (queue *w)
{
    int i;
    for (i = 0; i < w->size; ++i)
    {
        destroy_queue_element(&(w->elements[i]));
    }
    free(w->elements);
}

/**
 * Checks if given queue is initialized
 * */
boolean
is_queue_initialized(queue *w)
{
    return w->initialized;
}

/**
 * Initialize a queue.
 * 
 * @param w         Window to init
 * @param count     Number of cells in the queue
 * */
void
init_queue (queue *w, uint32_t count, uint32_t last_seq_num)
{
    if (TRUE == is_queue_initialized(w))
    {
        return;
    }

    LOGV("Initializing queue with size %d", count);
    w->size = (int)count;
    w->head = 0;
    w->tail = 0;
    w->elements = (queue_element_list) calloc(count, sizeof(queue_element));
    w->initialized = TRUE;
}

/**
 * Checks if queue is full
 * */
boolean
is_queue_full (queue *w)
{
    if (FALSE == is_queue_initialized(w))
    {
        return FALSE;
    }

    return ((w->head == w->tail) &&
            (NULL != w->elements[w->head].msg)) ? TRUE : FALSE;
}

/**
 * Checks if queue is empty
 * */
boolean
is_queue_empty (queue *w)
{
    if (FALSE == is_queue_initialized(w))
    {
        return FALSE;
    }

    return ((w->head == w->tail) &&
            (NULL == w->elements[w->head].msg)) ? TRUE : FALSE;
}

/**
 * Returns empty count of this queue i.e., number of empty cells
 * in the queue
 * */
uint32_t
get_empty_queue_count (queue *w)
{
    if (FALSE == is_queue_initialized(w) ||
            TRUE == is_queue_full(w))
    {
        return 0;
    }

    int occupied = (w->head - w->tail + w->size) % w->size;
    return w->size - occupied;
}

queue_element *
peek_tail (queue *w)
{
    return &(w->elements[w->tail]);
}

queue_element *
peek_head (queue *w)
{
    return &(w->elements[w->head]);
}

uint32_t
get_queue_element_seq_num (queue *w, int index)
{
    if (index < 0 || index >= w->size)
    {
        return -1;
    }

    msg_iovec msg = w->elements[index].msg;
    if (NULL != msg)
    {
        msg_header *h = msg[0].iov_base;
        return h->sequence_num;
    }

    return -1;
}
/**
 * Pushes a message onto the queue.
 *
 * @return Returns the index the mesasge was inserted in the queue
 *
 * *** NOTE ***
 * There is no copy of data here. Whatever message memory is given will be used.
 * */
int
push (queue *w, msg_iovec msg)
{
    if (FALSE == is_queue_initialized(w) ||
            TRUE == is_queue_full(w))
    {
        return -1;
    }

    int index;
    index = w->head;
    w->elements[w->head].msg = msg;
    w->elements[w->head].timestamp = get_current_system_time_millis();
    w->elements[w->head].transmission_count = 0;
    w->head += 1;
    w->head = w->head % w->size;

    return index;
}

boolean
is_hole (queue *w, int index)
{
    msg_iovec msg = w->elements[index].msg;
    if (NULL == msg)
    {
        return TRUE;
    }

    msg_header *hdr = msg[0].iov_base;
    return (-1 == hdr->payload_length) ? TRUE : FALSE;
}

msg_iovec
get_dummy_msg ()
{
    msg_iovec msg = create_msg(NULL, NULL, 0);
    return msg;
}

void
insert_dummy_at_tail (queue *w, uint32_t seq_num)
{
    msg_iovec msg = get_dummy_msg();
    msg_header *hdr = msg[0].iov_base;
    hdr->sequence_num = seq_num;
    w->elements[w->tail].msg = msg;
}

void
insert_dummy_at_tail_non_emptyq (queue *w)
{
    int i, j;
    msg_iovec msg;
    for (i = 1; i < w->size; ++i)
    {
        j = (w->tail + i) % w->size;
        if (NULL != w->elements[j].msg)
        {
            msg = w->elements[j].msg;
            break;
        }
    }

    msg_header *hdr = msg[0].iov_base;
    insert_dummy_at_tail(w, hdr->sequence_num - i);
}

int
push_at(queue *w, msg_iovec msg, uint32_t last_seq_num_recv)
{
    print_queue_stats(w);
    if (FALSE == is_queue_initialized(w) ||
            TRUE == is_queue_full(w))
    {
        return -1;
    }

    msg_header *header = msg[0].iov_base;
    msg_iovec msg_tail = w->elements[w->tail].msg;
    int index = -1;
    
    if (TRUE == is_queue_empty(w))
    {
        if (w->size < header->sequence_num - last_seq_num_recv)
        {
            return -1;
        }

        if (header->sequence_num == last_seq_num_recv)
        {
            LOGI("Already served: %d", last_seq_num_recv);
            return -1;
        }
        index = (w->tail + (header->sequence_num - last_seq_num_recv - 1) + w->size) % w->size;
        if (index != w->tail)
        {
            insert_dummy_at_tail(w, last_seq_num_recv + 1);
        }
        LOGV("-1Last sequence num recvd: %u This: %u index: %d", last_seq_num_recv, header->sequence_num, index);
    }
    else
    {
        msg_header *tail_header = msg_tail[0].iov_base;
        LOGV("Last sequence num recvd: %u This: %u Tail: %d index: %d", last_seq_num_recv, header->sequence_num, tail_header->sequence_num, index);
        if (w->size <= header->sequence_num - tail_header->sequence_num)
        {
            return -1;
        }

        index = (w->tail + (header->sequence_num - tail_header->sequence_num) + w->size) % w->size;
    }

    if (NULL != w->elements[index].msg)
    {
        msg_iovec index_msg = w->elements[index].msg;
        msg_header *h = (msg_header *)msg[0].iov_base;
        if (h->sequence_num != header->sequence_num)
        {
            return -1;
        }
        else
        {
            delete_msg(w->elements[index].msg);
        }
    }
    w->elements[index].msg = msg;
    w->elements[index].timestamp = get_current_system_time_millis();
    w->elements[index].transmission_count = 0;

    int index_tail_dist = (index - w->tail + w->size) % w->size;
    int head_tail_dist = (w->head - w->tail + w->size) % w->size;

    if (head_tail_dist <= index_tail_dist)
    {
        w->head = index + 1;
        w->head %= w->size;
    }

    return index;
}

int
first_hole (queue *w)
{    
    int i, j;
    msg_iovec msg;
    for (i = 0; i < w->size; ++i)
    {
        j = (i + w->tail) % w->size;
        if (TRUE == is_hole(w, j))
        {
            return j;
        }
    }

    return -1;
}

uint32_t
first_hole_sequence_num (queue *w)
{
    if (TRUE == is_queue_full(w))
    {
        return -1;
    }

    int index = first_hole(w);
    msg_iovec msg;
    msg_header *header;

    if (-1 != index)
    {
        index = (index - 1 + w->size) % w->size;
    }
    
    msg = (-1 == index) ? w->elements[w->head].msg : w->elements[index].msg;
    header = msg[0].iov_base;
    return header->sequence_num + 1;
}

uint32_t
get_payload_length (msg_iovec msg)
{
    msg_header *h = msg[0].iov_base;
    return h->payload_length;
}

void
ensure_state (queue *w)
{
    if (TRUE == is_queue_empty(w))
    {
        return;
    }

    if (NULL == w->elements[w->tail].msg)
    {
        insert_dummy_at_tail_non_emptyq(w);
    }
}

/**
 * Pops a message from the queue
 * */
boolean
pop (queue *w, queue_element *e)
{
    if (FALSE == is_queue_initialized(w) ||
            TRUE == is_queue_empty(w))
    {
        return FALSE;
    }

    if (TRUE == is_hole(w, w->tail))
    {
        if (NULL == w->elements[w->tail].msg)
        {
            insert_dummy_at_tail_non_emptyq(w);
        }
        return FALSE;
    }

    memcpy(e, &(w->elements[w->tail]), sizeof(queue_element));
    delete_msg(w->elements[w->tail].msg);
    memset(&(w->elements[w->tail]), 0, sizeof(queue_element));
    w->tail += 1;
    w->tail %= w->size;

    ensure_state(w);
    return TRUE;
}

/**
 * Pops a message from the queue
 * */
boolean
pop_msg (queue *w, msg_iovec *msg)
{
    if (FALSE == is_queue_initialized(w) ||
            TRUE == is_queue_empty(w))
    {
        return FALSE;
    }

    if (TRUE == is_hole(w, w->tail))
    {
        if (NULL == w->elements[w->tail].msg)
        {
            insert_dummy_at_tail_non_emptyq(w);
        }
        errno = EAGAIN;
        return FALSE;
    }

    *msg = w->elements[w->tail].msg;
    memset(&(w->elements[w->tail]), 0, sizeof(queue_element));
    w->tail += 1;
    w->tail %= w->size;

    ensure_state(w);
    return TRUE;
}
