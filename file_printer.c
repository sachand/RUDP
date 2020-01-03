#include "app_settings.h"
#include <fcntl.h>

static boolean g_serve = TRUE;
static sem_t g_sem;

static void
termination_handler (int sig, siginfo_t *si, void *unused)
{
    printf("\nCaught signal %s\n", signum_to_string(sig));
    switch(sig)
    {
    case SIGINT :
    case SIGHUP :
    case SIGTERM :
        sem_post(&g_sem);
    case SIGPIPE :
        g_serve = FALSE;
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
	sigaction(SIGPIPE, &new_action, NULL);
}

static int
ready_socket_count_pure (SOCKET sock, uint32_t ms)
{
    struct timeval timeout = ms_to_timeval(ms);

    fd_set rdfds;
    FD_ZERO(&rdfds);
    FD_SET(sock, &rdfds);

    return select(sock + 1, &rdfds, NULL, NULL, &timeout);
}

static int
set_fd_non_blocking(int fd, boolean non_blocking)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags = (TRUE == non_blocking) ? flags | O_NONBLOCK : flags & (~O_NONBLOCK);
    return fcntl(fd, F_SETFL, flags);
}

static void
do_relay (int pipe_fd)
{
    char *buf = (char *) malloc(FILE_BUFFER_READ_ONE_SHOT);
    set_fd_non_blocking(pipe_fd, TRUE);

    FILE *destination = stdout;

    while (TRUE == g_serve)
    {
        int rsc = ready_socket_count_pure((SOCKET)pipe_fd, 100);
        if (0 == rsc)
        {
            continue;
        }
        else if (0 > rsc)
        {
            break;
        }
        
        int received = read(pipe_fd, buf, FILE_BUFFER_READ_ONE_SHOT);
        if (0 < received)
        {
            if (1 != fwrite(buf, received, 1, destination))
            {
                break;
            }
            fflush(destination);
        }
        else
        {
            break;
        }
    }

    free(buf);
}

int
main (int argc, char *argv[])
{
    int pipe_fd;
    sscanf(argv[1], "%d", &pipe_fd);

    sem_init(&g_sem, 0, 0);
    initialize_signal_handler();

    do_relay(pipe_fd);
    close(pipe_fd);

    struct timespec t = get_abstime_after(XTERM_SLEEP_AFTER_COMPLETE_S * 1000);
    sem_timedwait(&g_sem, &t);

    return 0;
}
