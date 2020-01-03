#include "app_settings.h"

#define LOG_TAG "ClientConnect"

/**
 * Finds the best interface to connect a given address to and then
 * connects to it.
 * 
 * @param me            Endpoint to fill
 * @param server_ip     Dotted decimal notation of server/peer's IP
 * @param port_number   Peer'd port number to connect onto
 * 
 * @return Endpoint containing the connected socket.
 * */
void
client_connect (endpoint *me, char *server_ip, uint16_t port_number)
{
    endpoint_list if_list = get_unicast_endpoint_list();
    endpoint *t = if_list;
    uint32_t server_ip_n = 0;
    boolean loopback = FALSE;
    SOCKET sock = SOCKET_ERROR;

    inet_pton(AF_INET, server_ip, &server_ip_n);
    memset(me, 0, sizeof(endpoint));

    // Try to find a fast interface
    for (; NULL != t; t = t->next)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) &(t->network_address);
        if ((uint32_t)sin->sin_addr.s_addr == server_ip_n)
        {
            LOGS("Server intended to be on same host. Going on LOOPBACK");
            sock = bind_socket(0, NULL, TRUE);
            loopback = TRUE;
            break;
        }
        else if (TRUE == is_local_address_bare(t, ntohl(server_ip_n)))
        {
            LOGS("Server found on same subnet. Enabling DONTROUTE");
            sock = bind_socket(0, t->network_address, TRUE);
            break;
        }
    }

    // Didn't find a fast interface. Connect with anything
    if (SOCKET_ERROR == sock)
    {
        LOGS("Server not on the same subnet. Binding with arbitrary interface: %s",
                endpoint_to_string(if_list));
        sock = bind_socket(0, &(if_list->network_address), FALSE);
    }

    // Connect the socket to server's address.
    struct sockaddr_in peer;
    peer.sin_family = AF_INET;
    peer.sin_addr.s_addr = loopback == TRUE ? htonl(INADDR_LOOPBACK) : server_ip_n;
    peer.sin_port = htons(port_number);

    socklen_t addr_len = sizeof(struct sockaddr_in);
    EXEC_BARE(connect(sock, (struct sockaddr *)&peer, addr_len),
            "Setting connection to server failed.");

    // Get port number in local endpoint
    addr_len = sizeof(struct sockaddr_in);
    EXEC_BARE(getsockname(sock, (struct sockaddr *)&(me->network_address), &addr_len),
            "getsockname failed");

    t = (NULL == t) ? if_list : t;

    // Initialize endpoint
    // *** network_address is filled by getsockname() above. ***
    // *** DON'T OVERWRITE.***
    if (NULL != t)
    {
        me->sock = sock;
        me->subnet_mask = t->subnet_mask;
        me->next = NULL;
    }

    connection conn;
    conn.sock = me->sock;
    LOGV("Connected to server with: %s", to_string(&conn));
    free_unicast_endpoint_list(if_list);
}
