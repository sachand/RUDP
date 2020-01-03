#include "app_settings.h"

#define LOG_TAG "ServerHShake"

boolean
server_do_handshake (endpoint *listening, endpoint *connecting,
        struct sockaddr_in *destination, FILE **f)
{
    char file_name[MAX_FILENAME];
    uint16_t flags = 0;

    connection conn;
    conn.sock = listening->sock;
    LOGS("Listening Connection: %s", to_string(&conn));
    conn.sock = connecting->sock;
    LOGS("Connecting Connection: %s", to_string(&conn));
    
    // Read file name from the client.
    flags = MSG_HEADER_FLAG_FILENAME;
    EXEC_CTL(recv_rudp_ctl(listening, &flags, file_name, MAX_FILENAME),
            "Could not fetch file name from client.");
    if (MSG_HEADER_FLAG_FILENAME != flags)
    {
        LOGE("Error in connection");
        errno = EPROTO;
        return FALSE;
    }

    *f = fopen(file_name, "r");
    if (NULL == *f)
    {
        // File by given name not found. Exit
        LOGE("File \"%s\" not found. Informing client and quitting.", file_name);
        flags = MSG_HEADER_FLAG_ACK | MSG_HEADER_FLAG_FIM;
        send_rudp_ctl_int(listening, flags, NULL, 0, 0, 0);
        return FALSE;
    }

    // At this point we have found the file.
    // Send SYN.
    uint16_t connecting_port = connecting->network_address.sin_port;
    flags = MSG_HEADER_FLAG_SYN;

    ///@todo Rather die than submit this
    ///Suggested way is to create two threads but that was giving some
    ///problem with malloc-ed data. That is probably fixed now.
    ///Try out later.
    uint32_t timeout_ms = INITIAL_CTL_SEND_TIMEOUT_MS;
    uint16_t expected_flags;

    // It is important to send on connecting socket first
    // rather than listening although the other way round
    // is more senseful and natural.
    // However, by doing this internal channel setup becomes easy
    // without hampering the overall logic of protocol.
    expected_flags = MSG_HEADER_FLAG_FILENAME;
    send_rudp_ctl_int(connecting, flags, (char *)&connecting_port,
            sizeof(connecting_port), 0, 0);
    int retxmt_count = 0;
    do
    {
        send_rudp_ctl_int(listening, flags, (char *)&connecting_port,
                sizeof(connecting_port), 0, 0);

        send_rudp_ctl_int(connecting, flags, (char *)&connecting_port,
                sizeof(connecting_port), 0, 0);
                
        struct timeval timeout = ms_to_timeval(timeout_ms);

        fd_set rdfds;
        FD_ZERO(&rdfds);
        FD_SET(listening->sock, &rdfds);
        FD_SET(connecting->sock, &rdfds);
        SOCKET sock = listening->sock > connecting->sock ? listening->sock : connecting->sock;

        long start = get_current_system_time_millis();
        int rsc = select(sock + 1, &rdfds, NULL, NULL, &timeout);
        LOGV("RSC: %d SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSsss", rsc);

        if (0 > rsc)
        {
            LOGE("select failing. Error: %s", errno_string());
            unconnect_socket(listening->sock);
            close(listening->sock);
            return FALSE;
        }
        else if (0 < rsc)
        {
            if (0 != FD_ISSET(connecting->sock, &rdfds))
            {
                LOGV("sax Connecting");
                expected_flags = MSG_HEADER_FLAG_ACK | MSG_HEADER_FLAG_SYN;
                int r = recv_rudp_ctl_int(connecting, &expected_flags, NULL, 0, 0, 0);
                if (1 == r)
                {
                    break;
                }
                else if (0 > r)
                {
                    if (ECONNREFUSED == errno)
                    {
                        long left = timeout_ms - (get_current_system_time_millis() - start);
                        if (left > 0)
                        {
                            timeout = ms_to_timeval(left);
                            select(NULL, NULL, NULL, NULL, &timeout);
                        }
                    }
                }
                expected_flags = MSG_HEADER_FLAG_FILENAME;
            }
            if (0 != FD_ISSET(listening->sock, &rdfds))
            {
                LOGV("sax Listening");
                expected_flags = MSG_HEADER_FLAG_FILENAME;
                recv_rudp_ctl_int(listening, &expected_flags, NULL, 0, 0, 0);
            }
        }

        ++retxmt_count;
        timeout_ms <<= 1;
        LOGS("No response for SYN yet. Retries left: %d", CTL_RETRY_COUNT - retxmt_count);
    } while (retxmt_count != CTL_RETRY_COUNT);
    
    // No need for listening socket. Undo connect and close it.
    if (SOCKET_ERROR != listening->sock)
    {
        unconnect_socket(listening->sock);
        close(listening->sock);
    }

    if ((MSG_HEADER_FLAG_ACK | MSG_HEADER_FLAG_SYN) == expected_flags)
    {
        // All's cool. Go ahead.
    }
    else if (MSG_HEADER_FLAG_FIM == expected_flags)
    {
        LOGI("Server closed connection. Closing");
        return FALSE;
    }
    else if (MSG_HEADER_FLAG_FILENAME == expected_flags)
    {
        LOGI("Handshake timedout. Quitting.");
        errno = ETIMEDOUT;
        return FALSE;
    }
    else
    {
        // This is extra precaution. Ideally should not happen.
        // Above recv call should take care of that.
        LOGE("Unknown protocol state transition requested. Exiting");
        errno = EPROTO;
        return FALSE;
    }

    channel *c = find_channel(connecting->sock);
    c->sock = connecting->sock;
    print_channel_stats(c);

    LOGS("Handshake successful\n");
    return TRUE;
}
