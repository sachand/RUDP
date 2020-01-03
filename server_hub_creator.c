///@todo Consolidate with hub_manager

#include "app_settings.h"
#include "unpifiplus.h"

#define LOG_TAG "HubCreator"

/**
 * Creates the server hub.
 * 
 * This is where the code records and binds with appropriate interfaces.
 * */
endpoint_list
create_listen_hub (uint16_t port_number)
{
    endpoint_list if_list = get_unicast_endpoint_list();
    endpoint *t = if_list;

    while (NULL != t)
    {
        SOCKET sock = bind_socket(port_number, &(t->network_address), FALSE);
        if (SOCKET_ERROR == sock)
        {
            LOGE("Couldn't be bound to following endpoint. Skipping:%s",
                    endpoint_to_string(t));
            endpoint *temp = t->next;
            remove_endpoint(&if_list, t);
            t = temp;
            continue;
        }

        t->sock = sock;
        LOGS("Binding socket on %s", endpoint_to_string(t));
        t = t->next;
    }
    printf("\n");

    return if_list;
}
