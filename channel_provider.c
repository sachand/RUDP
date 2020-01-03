#include "app_settings.h"

#define LOG_TAG "ChannelProvider"

/**
 * Global list of all channels in the system currently for our protocol
 * */
static channel_list g_channels = NULL;

extern void *
do_retransmit (void *);

channel_id
channel_id_from_socket (SOCKET sock)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    channel_id id;

    getsockname(sock, (struct sockaddr *)&sin, &len);
    id.local_ip = ntohl(sin.sin_addr.s_addr);
    id.local_port = ntohs(sin.sin_port);

    len = sizeof(sin);
    getpeername(sock, (struct sockaddr *)&sin, &len);
    id.remote_ip = ntohl(sin.sin_addr.s_addr);
    id.remote_port = ntohs(sin.sin_port);

    return id;
}

boolean
is_channel_id_equal (channel_id *id1, channel_id *id2)
{
    return (id1->local_ip       == id2->local_ip    &&
            id1->local_port     == id2->local_port  &&
            id1->remote_ip      == id2->remote_ip   &&
            id1->remote_port    == id2->remote_port ) ? TRUE : FALSE;
}

static void
insert_channel (channel_list *list, channel *item)
{
    if (NULL != *list)
    {
        item->next = *list;
    }
    *list = item;
}

static boolean
remove_channel (channel_list *list, channel *item)
{
    channel *t, *s;
    for (t = *list, s = NULL; NULL != t; t = t->next)
    {
        if (TRUE == is_channel_id_equal(&(t->id), &(item->id)))
        {
            if (NULL == s)
            {
                *list = t->next;
            }
            else
            {
                s->next = t->next;
            }
            return TRUE;
        }
        s = t;
    }

    return FALSE;
}

channel *
get_new_channel (channel *id)
{
    channel *new_channel = (channel *) calloc (1, sizeof(channel));
    memcpy(&(new_channel->id), id, sizeof(channel_id));
    return new_channel;
}

channel *
find_channel (SOCKET sock)
{
    channel *t = g_channels;
    channel_id id_sock = channel_id_from_socket(sock);

    for (; NULL != t; t = t->next)
    {
        if (TRUE == is_channel_id_equal(&id_sock, &(t->id)))
        {
            break;
        }
    }

    if (NULL == t)
    {
        LOGV("Did not find channel");
        // Didn't find channel. Enter new one.
        channel *new_channel = get_new_channel(&id_sock);
        insert_channel(&g_channels, new_channel);
        t = new_channel;

        // Generate random Seq number for channel. Only one will survive
        // the process.
        t->next_seq_num_send = (uint32_t)lrand48();
        t->last_seq_num_recv = t->next_seq_num_send;
        t->stopping = FALSE;
    }

    return t;
}

void
change_channel_id (SOCKET sock, channel_id *old_id)
{
    channel *t = g_channels;
    channel_id new_id = channel_id_from_socket(sock);

    for (; NULL != t; t = t->next)
    {
        if (TRUE == is_channel_id_equal(old_id, &(t->id)))
        {
            LOGV("Found channel: %p", t);
            break;
        }
    }

    if (NULL != t)
    {
        t->id = new_id;
    }
}

void
print_channel_stats (channel *c)
{
    LOGV("Source window:");
    print_queue_stats(&(c->source.window));
    LOGV("Sink window:");
    print_queue_stats(&(c->sink.window));
    LOGV("Last seq num recv: %d Next seq num send: %d", c->last_seq_num_recv,
            c->next_seq_num_send);
}

void
init_send_channel (channel *c, uint32_t window_size)
{
    send_channel *sc = &(c->sink);
    init_queue(&(sc->window), window_size);
    sc->state.rwnd = window_size;

    pthread_mutexattr_t error_check_attr;
    pthread_mutexattr_init(&error_check_attr);
    pthread_mutexattr_settype(&error_check_attr, PTHREAD_MUTEX_ERRORCHECK);    
    pthread_mutex_init(&(sc->access_mutex), &error_check_attr);
    pthread_mutexattr_destroy(&error_check_attr);

    sem_init(&(sc->state.transmitted_something), 0, 0);
    pthread_cond_init(&(sc->state.stop_retransmission_alarm), NULL);
    init_congestion_state(sc);
    sc->active = TRUE;
    pthread_create(&(sc->state.retransmitter), NULL, &do_retransmit, c);
    init_retransmission_state(&(sc->state.retxmt_info));
}

void
init_recv_channel (recv_channel *rc, uint32_t window_size)
{
    init_queue(&(rc->window), window_size);

    pthread_mutexattr_t error_check_attr;
    pthread_mutexattr_init(&error_check_attr);
    pthread_mutexattr_settype(&error_check_attr, PTHREAD_MUTEX_ERRORCHECK);    
    pthread_mutex_init(&(rc->access_mutex), NULL);
    pthread_mutexattr_destroy(&error_check_attr);

    rc->active = TRUE;
}

send_channel *
get_send_channel (endpoint *p)
{
    channel *t = find_channel(p->sock);
    return &(t->sink);
}

recv_channel *
get_recv_channel (endpoint *p)
{
    channel *t = find_channel(p->sock);
    return &(t->source);
}

void
set_channel_stopping (SOCKET sock)
{
    channel *c = find_channel(sock);
    if (NULL != c)
    {
        c->stopping = TRUE;
    }
}

boolean
is_channel_stopping (SOCKET sock)
{
    channel *c = find_channel(sock);
    if (NULL != c)
    {
        return c->stopping;
    }

    return TRUE;
}

void
destroy_send_channel (channel *c)
{
    if (NULL == c)
    {
        return;
    }

    send_channel *sc = &(c->sink);
    if (FALSE == sc->active)
    {
        return;
    }

    // Signal retransmitter to die
    pthread_cancel(sc->state.retransmitter);
    pthread_join(sc->state.retransmitter, NULL);

    // Retransmitter dead
    pthread_cond_destroy(&(sc->state.stop_retransmission_alarm));
    sem_destroy(&(sc->state.transmitted_something));

    // Destroy retransmission state
    destroy_retransmission_state(&(sc->state.retxmt_info));

    // Destroy channel access handlers
    pthread_cond_destroy(&(sc->window_unlocked));
    pthread_mutex_destroy(&(sc->access_mutex));

    // Destroy Window
    destroy_queue(&(sc->window));

    memset(sc, 0, sizeof(send_channel));
    sc->active = FALSE;
}

void
destroy_recv_channel (channel *c)
{
    if (NULL == c)
    {
        return;
    }

    recv_channel *rc = &(c->source);
    if (FALSE == rc->active)
    {
        return;
    }

    // Destroy channel access handlers
    pthread_mutex_destroy(&(rc->access_mutex));

    // Destroy Window
    destroy_queue(&(rc->window));

    memset(rc, 0, sizeof(recv_channel));
    rc->active = FALSE;
}

int
destroy_channel (channel *c)
{
    if (NULL == c)
    {
        return 0;
    }

    recv_channel *rc = &(c->source);
    send_channel *sc = &(c->sink);

    if (NULL != rc)
    {
        destroy_recv_channel(c);
    }

    if (NULL != sc)
    {
        destroy_send_channel(c);
    }

    if (TRUE == remove_channel(&g_channels, c))
    {
        close(c->sock);
        free(c);
    }

    return 1;
}
