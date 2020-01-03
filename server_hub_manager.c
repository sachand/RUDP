#include "app_settings.h"

#define LOG_TAG "HubManager"

/**
 * Global that governs if hub is being managed
 * */
boolean g_serve = TRUE;

/**
 * Global list of all workers/server children present currently.
 * */
worker_list g_workers = NULL;

static void*
worker_remover (void *arg)
{
    pid_t *tag = (pid_t *)arg;
    remove_worker(&g_workers, *tag);
    free(arg);
    return NULL;
}

static void
termination_handler (int sig, siginfo_t *si, void *unused)
{
    pthread_t remover;
    void *arg;
	
    switch(sig)
	{
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
	case SIGPIPE:
        //LOGS("Caught signal %s. Terminating", signum_to_string(sig));
		g_serve = FALSE;
        break;

	case SIGCHLD:
        arg = malloc(sizeof(pid_t));
        memcpy(arg, &(si->si_pid), sizeof(pid_t));
        //LOGS("Caught signal %s. Child %d dead", signum_to_string(sig), si->si_pid);
        pthread_create(&remover, NULL, &worker_remover, arg);
        pthread_detach(remover);
        break;
	}
}

static void
initialize_signal_handler ()
{
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));

	new_action.sa_sigaction = termination_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &new_action, NULL);
	sigaction(SIGHUP, &new_action, NULL);
	sigaction(SIGTERM, &new_action, NULL);
	sigaction(SIGCHLD, &new_action, NULL);
	sigaction(SIGPIPE, &new_action, NULL);
}

/**
 * Handles incoming requests.
 * 
 * Creates a new process child to handle each new request.
 * */
boolean
handle_incoming_request (endpoint *me, endpoint_list list, struct sockaddr_in s_peer)
{
    if (TRUE == is_servicing(&g_workers, s_peer))
    {
        // Already servicing. Wait for some time here for the child process
        // to consume the contents in the buffer.
        sleep(1);
        return TRUE;
    }
    
	pid_t pID = fork();
	if (0 == pID)
	{
        handle_new_request(me, list, s_peer);
		exit(0);
	}
	else if (0 > pID)
	{
		LOGE("Could not fork. Error: %s", errno_string());
        return FALSE;
	}
    else
    {
        LOGI("Worker created: %d", pID);
        worker_id wid = worker_id_from_sockaddr(s_peer);
        worker *w = find_worker(g_workers, &wid);
        if (NULL != w)
        {
            w->tag = pID;
        }
    }

    return TRUE;
}

void
manage_loop(endpoint_list *list)
{
    fd_set rdfds;
    char buffer[MAX_FILENAME];
    WATCH_CALL(wait_for_ready_sockets(*list, &rdfds), "select");

    endpoint *t = NULL;
    for (t = *list; NULL != t; t = t->next)
    {
        if (0 == FD_ISSET(t->sock, &rdfds))
        {
            continue;
        }

        struct sockaddr_in sin;
        socklen_t sin_l = sizeof(sin);
        int bytes_read = recvfrom(t->sock, (void *)buffer, MAX_FILENAME, MSG_PEEK,
                (struct sockaddr *)&sin, &sin_l);

        if (0 < bytes_read)
        {
            g_serve = handle_incoming_request(t, *list, sin);
        }
    }
}

void
stop_hub(endpoint_list list)
{
    endpoint *t = list;
    for (; NULL != t; t = t->next)
    {
        close(t->sock);
    }

    free_unicast_endpoint_list(list);
}

void
manage_hub (endpoint_list list)
{
    initialize_signal_handler();

    while (TRUE == g_serve)
    {
        if (NULL == list)
        {
            LOGE("Nothing to manage. Quitting");
            break;
        }
        manage_loop(&list);
    }

    stop_hub(list);
    destroy_workers(g_workers);
}
