#include "app_settings.h"
#include <math.h>

#define LOG_TAG "Client"

/**
 * Global read only value for datagram loss probability
 * */
float g_dg_loss_probability = 0.0f;
static uint32_t g_read_gap_mean = 0;

/**
 * Pipe channel to relay file contents to xterm
 * */
int xterm_pipe_fd[2];

extern void
client_connect (endpoint *me, char *server_ip, uint16_t port_number);

extern void *
producer (void *args);

extern void *
do_remote_file_read (void *args);

boolean g_client_serve = TRUE;

static void
termination_handler (int sig, siginfo_t *si, void *unused)
{
    printf("\nCaught signal %s\n", signum_to_string(sig));
    switch(sig)
    {
    case SIGINT :
    case SIGHUP :
    case SIGTERM :
    case SIGPIPE :
        g_client_serve = FALSE;
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

/**
 * Read .in init file and initialize instance.
 * 
 * @param f                     File pointer to init file
 * @param server_ip             Server IP in dotted decimal
 * @param port_number           Pointer to service port number
 * @param file_name             File to fetch from server
 * @param window_size           Client's receive window size
 * @param seed                  Seed to give to randomizer
 * @param dg_loss_probability   Probability pf datagram loss
 * @param read_gap_mean         Mean of inter-read gap time in milliseconds
 * 
 * @return TRUE if parameter initialization successful, FALSE otherwise.
 * */
static boolean
init_specs (FILE *f, char *server_ip, uint16_t *port_number, char *file_name,
        uint32_t *window_size, unsigned int *seed)
{
    if (NULL == f)
    {
        LOGI("No init file found. Going with default init.");
        sprintf(server_ip, "%s", "127.0.0.1");
        *port_number = DEFAULT_PORT_NUMBER;
        sprintf(file_name, "%s", "server.in");        
        *window_size = DEFAULT_WINDOW_SIZE_RECEIVE;
        *seed = 0;
        return TRUE;
    }

    fscanf(f, "%s", server_ip);
    uint32_t temp;
    if (0 >= inet_pton(AF_INET, server_ip, &temp))
    {
        LOGE("Invalid IP in file: %s", server_ip);
        return FALSE;
    }

    fscanf(f, "%hu", port_number);
    if (0 >= *port_number)
    {
        LOGE("Invalid port number: %hu", *port_number);
        return FALSE;
    }

    ///@todo May be check if file name is legal
    fscanf(f, "%s", file_name);

    fscanf(f, "%u", window_size);
    if (0 >= *window_size)
    {
        LOGE("Invalid window size: %u", *window_size);
        return FALSE;
    }

    fscanf(f, "%u", seed);

    fscanf(f, "%f", &g_dg_loss_probability);
    if (0 > g_dg_loss_probability || 1 < g_dg_loss_probability)
    {
        LOGE("Invalid datagram loss probability: %u", g_dg_loss_probability);
        return FALSE;
    }

    fscanf(f, "%u", &g_read_gap_mean);
    if (0 > g_read_gap_mean)
    {
        LOGE("Invalid read gap mean: %u", g_read_gap_mean);
        return FALSE;
    }

    return TRUE;
}

/**
 * Whether drop should be simulated currently.
 * */
boolean
should_drop (msg_header *header, char *who)
{
    boolean drop = (drand48() > g_dg_loss_probability) ? FALSE : TRUE;
    if (TRUE == drop && NULL != header)
    {
        LOGS("Dropping during %s Sequence# %d Flags: %s", who,
                header->sequence_num, msg_header_flags_to_string(header->flags));
    }
    return drop;
}

/**
 * Introduces jittery sleep at client to simulate jittery latency in n/w
 * */
void
jitter_sleep ()
{
    double ms_to_sleep = -1 * (double)g_read_gap_mean * log(drand48());
    struct timeval t = ms_to_timeval((uint32_t)(ms_to_sleep + 0.5));
    select(NULL, NULL, NULL, NULL, &t);
}

pthread_t
handle_file_read (endpoint *p)
{
    pthread_t file_printer;
    pthread_create(&file_printer, NULL, &do_remote_file_read, p);
    return file_printer;
}

int
main (int argc, char *argv[])
{
    FILE *f = fopen ("client.in", "r");
    char server_ip[INET_ADDRSTRLEN];
    uint16_t port_number;
    char file_name[MAX_FILENAME];
    uint32_t window_size;
    unsigned int seed;

    memset(server_ip, 0, INET_ADDRSTRLEN);
    memset(file_name, 0, MAX_FILENAME);

    initialize_signal_handler();

    if (FALSE == init_specs(f, server_ip, &port_number, file_name, &window_size, &seed))
    {
        return 0;
    }

    srand48(seed);
    LOGI("Server IP: %s", server_ip);
    LOGI("Port Number: %hu", port_number);
    LOGI("File: %s", file_name);
    LOGI("Window Size: %u", window_size);
    LOGI("Seed: %u", seed);
    LOGI("Datagram Loss Probability: %f", g_dg_loss_probability);
    LOGI("Mean Read gap: %u", g_read_gap_mean);
    printf("\n");

    if (NULL != f)
    {
        fclose(f);
    }

    LOGV("Going for connect");
    endpoint p;
    client_connect(&p, server_ip, port_number);

    if (FALSE == client_do_handshake(&p, file_name, window_size))
    {
        LOGE("Handshake with server failed. Client quitting. Error: %s",
                errno_string());
        return 0;
    }

    // Handshake successful. Start file reception.
    channel *c = find_channel(p.sock);
    pipe(xterm_pipe_fd);
    pid_t pID = fork();
	if (0 == pID)
	{
		close(xterm_pipe_fd[1]);
        close(p.sock);
        char fd_str[20];
        sprintf(fd_str, "%d", xterm_pipe_fd[0]);
        char *argv[] = { "xterm", "-e", "./file_printer", fd_str, '\0' };

        int res = execvp(argv[0], argv);
        if (0 > res)
        {
            LOGE("exec failed. Error: %s\n", errno_string());
        }
        
        close(xterm_pipe_fd[0]);
		exit(0);
	}
	else if (0 > pID)
	{
		LOGE("Could not fork. Error: %s\n", errno_string());
	}
	else
	{
		close(xterm_pipe_fd[0]);
        pthread_t extractor = handle_file_read(&p);
        channel *c = find_channel(p.sock);
        producer(c);
        c->stopping = TRUE;
        g_client_serve = FALSE;

        pthread_join(extractor, NULL);
        close(p.sock);
	}

    destroy_channel(c);

    return 0;
}
