#include "app_settings.h"

pthread_mutex_t workers_lock = PTHREAD_MUTEX_INITIALIZER;

static boolean
is_worker_equal (worker_id *id1, worker_id *id2)
{
    return (id1->remote_ip      == id2->remote_ip   &&
            id1->remote_port    == id2->remote_port ) ? TRUE : FALSE;
}

static void
__insert_worker (worker_list *list, worker *item)
{
    if (NULL != *list)
    {
        item->next = *list;
    }
    *list = item;
}

worker_id
worker_id_from_sockaddr (struct sockaddr_in sin)
{
    worker_id id;
    id.remote_ip = (uint32_t)sin.sin_addr.s_addr;
    id.remote_port = (uint16_t)sin.sin_port;

    return id;
}

void
destroy_worker (worker *w)
{
    if (NULL != w)
    {
        free(w);
    }
}

void
remove_worker (worker_list *list, pid_t tag)
{
    pthread_mutex_lock(&workers_lock);
        worker *prev = NULL;
        worker *t = *list;

        for (; NULL != t; t = t->next)
        {
            if (t->tag == tag)
            {
                if (NULL != prev)
                {
                    prev->next = t->next;
                }
                else
                {
                    *list = t->next;
                }
                destroy_worker(t);
                break;
            }
            prev = t;
        }
    pthread_mutex_unlock(&workers_lock);
}

worker *
find_worker (worker_list list, worker_id *id)
{
    worker *t = list;
    for (; NULL != t; t = t->next)
    {
        if (is_worker_equal(&(t->id), id))
        {
            return t;
        }
    }

    return NULL;
}

boolean
is_servicing (worker_list *list, struct sockaddr_in s_peer)
{
    boolean status = TRUE;
    pthread_mutex_lock(&workers_lock);
        worker_id id = worker_id_from_sockaddr(s_peer);
        worker *w = find_worker(*list, &id);
        if (NULL == w)
        {
            w = (worker *) calloc(1, sizeof(worker));
            w->id = id;
            __insert_worker(list, w);
            status = FALSE;
        }
    pthread_mutex_unlock(&workers_lock);
    return status;
}

void
destroy_workers (worker_list list)
{
    pthread_mutex_lock(&workers_lock);
        worker *t, *s;

        for (t = list; NULL != t;)
        {
            s = t;
            t = t->next;
            destroy_worker(s);
        }
    pthread_mutex_unlock(&workers_lock);
}
