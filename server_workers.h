#pragma once

/**
 * Identifier for each worker.
 * This stands for the remote endpoint
 * */
typedef struct worker_id_t
{
    uint32_t remote_ip;
    uint16_t remote_port;
} worker_id;

/**
 * Details of a worker
 * */
typedef struct worker_t
{
    worker_id id;
    pid_t tag;

    struct worker_t *next;
} worker;

typedef worker *worker_list;

worker_id
worker_id_from_sockaddr (struct sockaddr_in sin);

void
remove_worker (worker_list *list, pid_t tag);

worker *
find_worker (worker_list list, worker_id *id);

boolean
is_servicing (worker_list *list, struct sockaddr_in s_peer);

void
destroy_worker (worker *w);

void
destroy_workers (worker_list list);
