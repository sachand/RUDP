#include "app_settings.h"
#include "channel_provider.h"

#define LOG_TAG "ClientHShake"

extern float g_dg_loss_probability;
/**
 * Performs the handshake from client's side
 * 
 * @param p             Endpoint to shake hands on
 * @param file_name     File name that needs to be received
 * @param window_size   Size for recv windoe read from client.in
 * 
 * @return Whether handshake successful
 * */
boolean
client_do_handshake (endpoint *p, char *file_name, uint32_t window_size)
{
    uint32_t file_name_len = strlen(file_name);
    uint16_t flags = MSG_HEADER_FLAG_FILENAME;
    connection conn;
    conn.sock = p->sock;
    LOGS("Initiating handshake on: %s", to_string(&conn));

    // Send file name to server till something comes on the connection
    EXEC_CTL(send_rudp_ctl(p, flags, file_name, file_name_len),
            "Could not send file name to server.");

    // Something was read on the socket. Read it
    uint16_t new_port = 0;
    flags = MSG_HEADER_FLAG_SYN;
    EXEC_CTL((recv_rudp_ctl(p, &flags, (char *)&new_port, sizeof(new_port))),
            "Did not receive new port number for file transfer from server.");

    // Store channel id for this connection.
    channel_id old_id;
    old_id = channel_id_from_socket(p->sock);

    if (TRUE == is_flag_set(flags, MSG_HEADER_FLAG_FIM))
    {
        LOGI("File not found at server. Exiting");
        errno = EBADF;
        return FALSE;
    }
    else if (MSG_HEADER_FLAG_SYN == flags)
    {
        // Successful. Now connect with new port
        struct sockaddr_in sin_peer;
        socklen_t len = sizeof(struct sockaddr_in);
        getpeername(p->sock, (struct sockaddr *)&sin_peer, &len);
        sin_peer.sin_family = AF_INET;
        sin_peer.sin_port = new_port;

        LOGV("Old Connection: %s", to_string(&conn));
        EXEC_CTL(connect(p->sock, (struct sockaddr *)&sin_peer, (socklen_t)sizeof(struct sockaddr_in)),
                "Could not connect on new connection from server");
        LOGV("New Connection: %s", to_string(&conn));
        LOGS("Got new port from server: %u. Switching connection.", ntohs(new_port));
    }
    else
    {
        LOGE("Unknown protocol state transition requested. Exiting");
        errno = EPROTO;
        return FALSE;
    }

    // By now, server has SYN-ed us. This was done on old connection.
    // This means underlying channels created in response to SYN
    // need to be re-linked to this new connection.
    change_channel_id(p->sock, &old_id);

    ///@todo Rather die than submit this
    // Acknowledge to new connection
    uint32_t timeout_ms = INITIAL_CTL_SEND_TIMEOUT_MS;
    uint16_t expected_flags;
    flags = MSG_HEADER_FLAG_SYN | MSG_HEADER_FLAG_ACK;
    int retxmt_count = 0;
    boolean syn_ack_sent = FALSE;
    do
    {
        // Send server a SYN-ACK. And wait for something to arrive.
        if (SOCKET_ERROR != send_rudp_ctl_int(p, flags, &(window_size),
                sizeof(window_size), 0, timeout_ms))
        {
            if (0 == peek_remove_rudp_ctl_one_shot(p->sock, MSG_HEADER_FLAG_SYN, 0, NULL, 0))
            {
                syn_ack_sent = TRUE;
                break;
            }
        }

        timeout_ms <<= 1;
        ++retxmt_count;
        LOGS("No response for SYN_ACK yet. Retries left: %d", CTL_RETRY_COUNT - retxmt_count);
    } while (retxmt_count != CTL_RETRY_COUNT);

    if (FALSE == syn_ack_sent)
    {
        LOGE("Could not send SYN_ACK to server. Quitting");
        errno = ETIMEDOUT;
        return FALSE;
    }

    channel *c = find_channel(p->sock);
    c->sock = p->sock;
    print_channel_stats(c);

    LOGS("Handshake successful\n");
    return TRUE;
}
