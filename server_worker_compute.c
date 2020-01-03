#include "app_settings.h"

#define LOG_TAG "WorkerCompute"

extern boolean g_serve;

/**
 * Cleans the listening socket. This is important for a concurrent server
 * */
static void
listen_socket_cleanup (SOCKET sock)
{
    unconnect_socket(sock);
    close(sock);
}

/**
 * A Client has arrived. Serve it.
 * 
 * THIS MUST BE CALLED IN ITS OWN PROCESS.
 * 
 * @param me            Endpoint with which server parent was contacted
 * @param list          Endpoint list of all interfaces. Needed so that
 *                      child closes all sockets it doesn't need
 * @param s_peer        sockaddr_in struct of client/peer
 * */
void
handle_new_request (endpoint *me, endpoint_list list, struct sockaddr_in s_peer)
{
    srand48(time(NULL));

    // First close all sockets you do not want.
    endpoint *temp = NULL;
    for (temp = list; NULL != temp; temp = temp->next)
    {
        if (temp->sock != me->sock)
        {
            close(temp->sock);
        }
    }
    
    LOGS("Begin server child compute");

    // Reconnect child's socket... But why?
    ///@todo revisit if really needed.
    socklen_t addr_len = sizeof(struct sockaddr_in);
    EXEC_BARE(connect(me->sock, (struct sockaddr *)&s_peer, addr_len),
            "Setting connection to client failed.");
    
    LOGV("Going to bind");
    // Bind to a socket using the i/f client pinged us at.
    struct sockaddr_in new_me;
    memset(&new_me, 0, sizeof(struct sockaddr_in));
    new_me.sin_addr.s_addr = me->network_address.sin_addr.s_addr;
    boolean local = is_local_address(me, &s_peer);
    if (TRUE == local)
    {
        LOGS("Remote IN local subnet. Setting dont route");
    }
    else
    {
        LOGS("Remote NOT IN local subnet. NOT setting dont route");
    }
    
    SOCKET sock = bind_socket(0, (struct sockaddr *)&(new_me),
            local);
    if (SOCKET_ERROR == sock)
    {
        LOGE("Child exiting as socket bind failed.");
        return;
    }

    // Connect the socket with client's address.
    addr_len = sizeof(struct sockaddr_in);
    EXEC_BARE(connect(sock, (struct sockaddr *)&s_peer, addr_len),
            "Setting connection to client failed.");

    // Create new endpoint for the new port
    endpoint hand;
    hand.sock = sock;
    hand.subnet_mask = me->subnet_mask;
    hand.next = NULL;
    
    addr_len = sizeof(struct sockaddr_in);
    EXEC_BARE(getsockname(sock, (struct sockaddr *)&(hand.network_address), &addr_len),
            "getsockname failed");

    // Time to shake hands
    FILE *f = NULL;
    if (FALSE == server_do_handshake(me, &hand, &s_peer, &f))
    {
        LOGE("Connection did not succeed between server child and client. Error: %s",
                errno_string());
        listen_socket_cleanup(me->sock);
        return;
    }

    // Handshake successful. Now start the transfer and stuff.
    pthread_t listener = recv_channel_start(&hand, &g_serve);
    send_channel *sc = get_send_channel(&hand);

    file_transfer(f, &hand);
    pthread_join(listener, NULL);
    fclose(f);

    destroy_channel(find_channel(hand.sock));

    // Wait for 2 MSLs.
    LOGS("2*MSL Wait Start");
    struct timeval t = ms_to_timeval(2 * MSL_MS);
    select(NULL, NULL, NULL, NULL, &t);
    LOGS("2*MSL Wait End");
}
