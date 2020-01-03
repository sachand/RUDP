/**
 * @author Saksham Chand <saksham.chand@stonybrook.edu>
 * */

#include "app_settings.h"

#define LOG_TAG "SockCommon"

#if SIMULATE_DROP == 1
    extern boolean
    should_drop (msg_header *header, char *who);
#else
    boolean
    should_drop (msg_header *header, char *who)
    {
        return FALSE;
    }
#endif

inline int
set_socket_reuseaddr(SOCKET sock)
{
	int optval = 1;
	return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &optval,
			sizeof(optval));
}

inline int
set_socket_no_route(SOCKET sock)
{
	int optval = 1;
	return setsockopt(sock, SOL_SOCKET, SO_DONTROUTE, (char *) &optval,
			sizeof(optval));
}

int
ready_socket_count_pure (SOCKET sock, uint32_t ms)
{
    struct timeval timeout = ms_to_timeval(ms);

    fd_set rdfds;
    FD_ZERO(&rdfds);
    FD_SET(sock, &rdfds);

    return select(sock + 1, &rdfds, NULL, NULL, &timeout);
}

int
ready_socket_count(SOCKET sock, int ms)
{
    struct msghdr msg_msghdr;
    memset(&msg_msghdr, 0, sizeof(struct msghdr));

    msg_iovec r_msg = create_msg_max();
    msg_msghdr.msg_iov = r_msg;
    msg_msghdr.msg_iovlen = 2;

    int bytes_read = 0;
    uint64_t start = 0;

    do
    {
        if (0 == start)
        {
            start = get_current_system_time_millis();
        }
        else
        {
            bytes_read = 0;
            ms -= (int)(get_current_system_time_millis() - start);
            if (0 > ms)
            {
                break;
            }
        }

        int rsc = ready_socket_count_pure(sock, ms);
        if (0 > rsc)
        {
            bytes_read = SOCKET_ERROR;
            break;
        }
        else if (0 < rsc)
        {
            if (FALSE == should_drop(NULL, "RECV"))
            {
                bytes_read = 1;
                break;
            }
            else
            {
                bytes_read = recvmsg(sock, &msg_msghdr, MSG_DONTWAIT);
                if (0 > bytes_read)
                {
                    LOGE("Recv failed even after select succeeded. Error: %s",
                            errno_string());
                    break;
                }
                else if (0 == bytes_read)
                {
                    break;
                }
                else
                {
                    msg_header *header = (msg_header *)r_msg[0].iov_base;
                    LOGS("Dropping during RECV Sequence# %d Flags: %s", header->sequence_num,
                            msg_header_flags_to_string(header->flags));
                }
            }
        }
        else
        {
            errno = ETIMEDOUT;
            bytes_read = 0;
            break;
        }
    } while (1);

    delete_msg(r_msg);
    return bytes_read;
}

int
wait_for_ready_socket(SOCKET sock)
{
    fd_set rdfds;
    FD_ZERO(&rdfds);
    FD_SET(sock, &rdfds);

    return select(sock + 1, &rdfds, NULL, NULL, NULL);
}

int
wait_for_ready_sockets(endpoint_list list, fd_set *rdfds)
{
    FD_ZERO(rdfds);
    int max_fd = 0;

    endpoint *temp;
    for (temp = list; NULL != temp; temp = temp->next)
    {
        FD_SET(temp->sock, rdfds);
        max_fd = max_fd < temp->sock ? temp->sock : max_fd;
    }

    struct timeval timeout = ms_to_timeval(5000);
    return select(max_fd + 1, rdfds, NULL, NULL, &timeout);
}

void
unconnect_socket (SOCKET sock)
{
    struct sockaddr_in sin_unconnect;
    memset(&sin_unconnect, 0, sizeof(struct sockaddr_in));
    sin_unconnect.sin_family = AF_UNSPEC;
    
    connect(sock, &sin_unconnect, sizeof(struct sockaddr_in));
}

/**
 * Core method that write on the socket. This function is non-blocking.
 * 
 * @param sock          Socket fd to write to
 * @param msg           Message to write
 * 
 * @return On success, this call returns the number of characters sent.
 * On error, -1 is returned, and errno is set appropriately.
 * */
uint32_t
socket_send_msg_default (SOCKET sock, msg_iovec msg)
{
    if (NULL == msg)
    {
        return 0;
    }

    struct msghdr msg_msghdr;
    memset(&msg_msghdr, 0, sizeof(struct msghdr));
    msg_msghdr.msg_iov = msg;
    msg_msghdr.msg_iovlen = 2;
    msg_header *h = msg[0].iov_base;

    if (FALSE == should_drop(h, "SEND"))
    {
        print_msg_header(h, "SEND");
        return sendmsg(sock, &msg_msghdr, MSG_DONTWAIT);
    }
    else
    {
        return HEADER_SIZE + h->payload_length;
    }
}

/**
 * Core method that reads from the socket.
 * 
 * @param sock          Socket fd to read from
 * @param msg           Pointer to message this function fills
 * @param timeout_ms    Time to wait for read
 * 
 * @return If positive, it tells the number of  bytes read.
 * If 0 is returned then it implies nothing to read within the tineout
 * If SOCKET_ERROR is returned, it implies an error. Caller should read
 * errno
 * */
uint32_t
socket_recv_msg_default (SOCKET sock, msg_iovec *msg, int timeout_ms)
{
    struct msghdr msg_msghdr;
    memset(&msg_msghdr, 0, sizeof(struct msghdr));

    msg_iovec r_msg = create_msg_max();
    msg_msghdr.msg_iov = r_msg;
    msg_msghdr.msg_iovlen = 2;

    int bytes_read = 0;
    uint64_t start = 0;

    LOGV("Waiting for RSC: %d", timeout_ms);
    int rsc = ready_socket_count(sock, timeout_ms);
    if (1 == rsc)
    {
        bytes_read = recvmsg(sock, &msg_msghdr, MSG_DONTWAIT);
        if (0 > bytes_read)
        {
            LOGE("Recv failed even after select succeeded. Error: %s",
                    errno_string());
        }
        else if (0 < bytes_read)
        {
            print_msg_header(r_msg[0].iov_base, "RECV");
        }
    }
    else if (0 > rsc)
    {
        bytes_read = SOCKET_ERROR;
    }  
    else
    {
        errno = ETIMEDOUT;
        bytes_read = 0;
    }

    if (0 >= bytes_read)
    {
        delete_msg(r_msg);
    }
    else
    {
        *msg = r_msg;
    }

    return bytes_read;
}

/**
 * Core method that peeks from the socket in one shot.
 * 
 * @param sock          Socket fd to peek from
 * @param msg           Pointer to message this function fills
 * 
 * @return If positive, it tells the number of  bytes read.
 * If 0 is returned then it implies nothing to read within the tineout
 * If SOCKET_ERROR is returned, it implies an error. Caller should read
 * errno
 * */
uint32_t
socket_peek_msg_default (SOCKET sock, msg_iovec *msg)
{
    struct msghdr msg_msghdr;
    memset(&msg_msghdr, 0, sizeof(struct msghdr));

    msg_iovec r_msg = create_msg_max();
    msg_msghdr.msg_iov = r_msg;
    msg_msghdr.msg_iovlen = 2;

    int bytes_read = 0;

    int rsc = ready_socket_count(sock, 0);
    if (1 == rsc)
    {
        bytes_read = recvmsg(sock, &msg_msghdr, (MSG_DONTWAIT | MSG_PEEK));
        if (0 > bytes_read)
        {
            LOGE("Recv failed even after select succeeded. Error: %s",
                    errno_string());
        }
    }
    else if (0 > rsc)
    {
        bytes_read = SOCKET_ERROR;
    }  
    else
    {
        errno = ETIMEDOUT;
        bytes_read = 0;
    }

    if (0 >= bytes_read)
    {
        delete_msg(r_msg);
    }
    else
    {
        *msg = r_msg;
    }

    return bytes_read;
}
