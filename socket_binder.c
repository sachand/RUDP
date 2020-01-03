///@todo Move into socket_common.c

#include "app_settings.h"

#define LOG_TAG "SocketBinder"

/**
 * Error codes for bind, accept and listen calls. On failure these calls
 * return these and set errno for the detailed message
 * */
#define BIND_FAILED -1

/**
 * Error log format for create
 * */
#define CREATE_FAIL_LOG_FORMAT "Socket %s failed. Port: %d. Error: %s"

/**
 * Macro that essentially logs out an error message of format CREATE_FAIL_LOG_FORMAT,
 * closes socket and frees heap memory used, if any.
 *
 * @param _x_	Failure message to log
 * @param _y_ 	Port number on which operation was intended
 * @param _z_ 	Socket fd
 * */
#define CLEAN_CREATE(_x_, _y_, _z_) 								\
	do 																\
	{ 																\
		LOGE(CREATE_FAIL_LOG_FORMAT, _x_, _y_, errno_string()); 	\
        close(_z_);                                                 \
		return SOCKET_ERROR;    									\
	} while (0); 													\

/**
 * Simple wrapper over socket create and bind. This creates a socket and
 * binds it according to information provided. Only for IPv4.
 * 
 * @param port_number       The port to bind the socket to
 * @param sin               sockaddr_in struct of i/f to bind. It this is
 *                          NULL, loopback is assumed.
 * @param dont_route        Whether to set SO_DONTROUTE option
 * 
 * @return Socket fd of bound socket if successful. SOCKET_ERROR otherwise.
 * */
SOCKET
bind_socket (uint16_t port_number, struct sockaddr_in *sin, boolean dont_route)
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SOCKET_ERROR == sock)
    {
		CLEAN_CREATE("create", port_number, sock);
	}

	set_socket_reuseaddr(sock);
    if (TRUE == dont_route)
    {
        set_socket_no_route(sock);
    }

    // Assume loopback
    if (NULL == sin)
    {
        struct sockaddr_in loopback;
        loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sin = &loopback;
    }
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port_number);

    if (BIND_FAILED == bind(sock, (struct sockaddr *)sin, sizeof(struct sockaddr_in)))
    {
        CLEAN_CREATE("bind", port_number, sock);
    }

    return sock;
}
