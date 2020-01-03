#include "app_settings.h"
#include "socket_common.h"
#include "extras.h"
#include "unpifiplus.h"
#include "endpoint.h"

#define LOG_TAG "Endpoint"

/**
 * Checks if endpoint is valid
 * */
boolean
is_valid (endpoint *p)
{
    return (NULL == &(p->network_address)) ? TRUE : FALSE;
}

/**
 * Checks if interface is UP
 * */
boolean
is_up (struct ifi_info *ifc)
{
    return is_flag_set((uint32_t)(ifc->ifi_flags), IFF_UP);
}

/**
 * Checks if ifi_info is unicast
 * */
boolean
is_unicast (struct ifi_info *ifc)
{
    return (ifc->ifi_addr == NULL) ? FALSE : TRUE;
}

/**
 * Checks if interface is wildcard
 * */
boolean
is_wildcard (struct ifi_info *ifc)
{
    struct sockaddr_in *sin = (struct sockaddr_in *) ifc->ifi_addr;
    return (0 == sin->sin_addr.s_addr) ? TRUE : FALSE;
}

/**
 * Removes AND destroys an endpoint entry from given list.
 * */
void
remove_endpoint (endpoint_list *list, endpoint *record)
{
    if (record == *list)
    {
        *list = record->next;
    }
    else
    {
        endpoint *t, *s;
        for (t = *list; NULL != t; t = t->next)
        {
            if (t == record)
            {
                s->next = t->next;
                break;
            }
            s = t;
        }
    }

    free(record);
}

/**
 * Inserts an endpoint in the given list in decreasing order of
 * subnet mask
 * */
static void
insert_endpoint (endpoint_list *list, endpoint *record)
{
    if (NULL == record)
    {
        return;
    }

    if (NULL == *list)
    {
        *list = record;
        return;
    }

    endpoint *t, *prev = NULL;
    boolean recorded = FALSE;
    for (t = *list; NULL != t; t = t->next)
    {
        if (t->subnet_mask < record->subnet_mask)
        {
            recorded = TRUE;
            if (NULL != prev)
            {
                prev->next = record;
            }
            else
            {
                *list = record;
            }
            record->next = t;
            break;
        }
        prev = t;
    }

    if (FALSE == recorded)
    {
        prev->next = record;
        record->next = NULL;
    }
}

/**
 * Creates and returns a list of all appropriate endpoints - up, unicast
 * and not wildcard.
 * */
endpoint_list
get_unicast_endpoint_list ()
{
    struct ifi_info *head = Get_ifi_info_plus(AF_INET, 1);
    endpoint_list if_list = NULL;

    struct ifi_info *t = NULL;
    for (t = head; NULL != t; t = t->ifi_next)
    {
		LOGI("Analyzing interface: %s:%d", t->ifi_name, t->ifi_index);

        if (FALSE == is_up(t))
        {
            LOGI("Interface %s:%d is NOT UP. Skipping", t->ifi_name, t->ifi_index);
            continue;
        }

        if (FALSE == is_unicast(t))
        {
            LOGI("Interface %s:%d does NOT have UNICAST. Skipping", t->ifi_name, t->ifi_index);
            continue;
        }

        if (TRUE == is_wildcard(t))
        {
            LOGI("Interface %s:%d IS WILDCARD. Skipping", t->ifi_name, t->ifi_index);
            continue;
        }

        LOGI("Interface %s:%d IS UP, HAS UNICAST and IS NOT WILDCARD",
                t->ifi_name, t->ifi_index);
        struct sockaddr_in *sin = (struct sockaddr_in *) t->ifi_addr;
        endpoint *if_record = (endpoint *) calloc(1, sizeof(endpoint));

        if_record->sock = -1;
        memcpy(&(if_record->network_address), sin, sizeof(struct sockaddr_in));

        sin = (struct sockaddr_in *) t->ifi_ntmaddr;
        if_record->subnet_mask = ntohl(sin->sin_addr.s_addr);

        insert_endpoint(&if_list, if_record);
    }

    printf("\n");
    free_ifi_info_plus(head);
    return if_list;
}

/**
 * Destroys the given endpoint list
 * */
void
free_unicast_endpoint_list (endpoint_list l)
{
    if (NULL == l->next)
    {
        free(l);
        return;
    }

    endpoint *t;
    for (t = l->next; NULL != t; t = t->next)
    {
        free(l);
        l = t;
    }

    free(l);
}

/**
 * Utility function to get human readable description of given endpoint
 * */
char *
endpoint_to_string (endpoint *p)
{
    int i = 1;
    int mask = 32;
    while (0 == (p->subnet_mask & i))
    {
        --mask;
        i <<= 1;
    }

    char *temp = p->desc;

    temp += sprintf(temp, "Network Address: ");
    inet_ntop(AF_INET, &(p->network_address.sin_addr), temp, INET_ADDRSTRLEN);
    while ('\0' != *temp) { ++temp; }

    temp += sprintf(temp, "\/%u", mask);
    *temp = '\0';

    return (p->desc);
}

/**
 * Checks if given address is local to the given endpoint
 * Address must be given in host order.
 * */
boolean
is_local_address_bare (endpoint *p, uint32_t remote_addr)
{
    if (INADDR_LOOPBACK == remote_addr)
    {
        return TRUE;
    }

    uint32_t remote_masked = remote_addr & p->subnet_mask;
    uint32_t self_masked = ntohl(p->network_address.sin_addr.s_addr) & p->subnet_mask;

    return remote_masked == self_masked ? TRUE : FALSE;
}

/**
 * Checks if given sockaddr_in is local to given endpoint
 * */
boolean
is_local_address (endpoint *p, struct sockaddr_in *sin)
{
    return is_local_address_bare(p, ntohl(sin->sin_addr.s_addr));
}

/**
 * Utility function to fill given connection based on its socket
 * */
void
fill_connection_details (connection *conn)
{
    socklen_t addr_len;
    struct sockaddr_in addr;

    addr_len = sizeof(addr);
    memset(&addr, 0, addr_len);
    if (SOCKET_ERROR != getpeername(conn->sock, (struct sockaddr *) &addr, &addr_len))
    {
		inet_ntop(AF_INET, &(addr.sin_addr), conn->client, INET_ADDRSTRLEN);
		conn->client_port = ntohs(addr.sin_port);
    }

    addr_len = sizeof(addr);
    memset(&addr, 0, addr_len);
    if (SOCKET_ERROR != getsockname(conn->sock, (struct sockaddr *) &addr, &addr_len))
    {
		inet_ntop(AF_INET, &(addr.sin_addr), conn->server, INET_ADDRSTRLEN);
		conn->server_port = ntohs(addr.sin_port);
    }
}

/**
 * Utility function to get human readable description of given connection
 * */
char *
to_string (connection *conn)
{
    fill_connection_details(conn);
	sprintf(conn->desc, "%s:%d %s:%d", conn->server, conn->server_port,
			conn->client, conn->client_port);
	return conn->desc;
}

/**
 * Utility function to print human readable description of given socket
 * */
void
print_sock_stats (SOCKET sock, char *prefix)
{
    connection conn;
    conn.sock = sock;
    LOGS("%s Connection: %s", prefix, to_string(&conn));
}

/**
 * Utility function to print human readable description of given endpoint
 * */
void
print_endpoint_stats (endpoint *p, char *prefix)
{
    print_sock_stats(p->sock, prefix);
}
