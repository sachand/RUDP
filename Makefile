CC = gcc

CFLAGS = -w -g -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

RM = rm -f

SYSTEM_LIBS = \
	-lresolv \
	-lnsl \
	-lpthread \
	-lm \
	-lsocket \
	-lrt \
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

TARGET_LIBS = \
	server \
	client \
	prifinfo_plus \
	file_printer \

OBJECTS = \
	server.o \
	client.o \

#The original utils
SOURCES_UTILS = \
	logger.c \
	np_time.c \
	extras.c \

#Shared files with headers
SOURCES_COMMON = \
	logger.c \
	np_time.c \
	extras.c \
	queue.c \
	channel_provider.c \
	retransmission_timer_6298.c \
	msg_header.c \
	socket_common.c \
	server_workers.c \

HEADERS_COMMON = $(SOURCES_COMMON:.c=.h)

#Shared file but no headers
SOURCES_BASE = \
	get_ifi_info_plus.c \
	endpoint.c \
	ctl_message_handler.c \
	channel_wrapper.c \
	socket_binder.c \
	congestion_control_5681_2581.c \
	recv_channel_handler.c \
	send_channel_handler.c \
	retransmitter_prober.c \

SOURCES_SERVER = \
	${SOURCES_COMMON} \
	${SOURCES_BASE} \
	server.c \
	server_handshake.c \
	server_hub_creator.c \
	server_hub_manager.c \
	server_worker_compute.c \
	file_sender.c \

SOURCES_CLIENT = \
	${SOURCES_COMMON} \
	${SOURCES_BASE} \
	client.c \
	client_handshake.c \
	client_server_connector.c \
	file_extractor.c \

.PHONY: all
all: ${TARGET_LIBS} clean

server : server.o
	${CC} ${CFLAGS} -o server ${SOURCES_SERVER:.c=.o} ${SYSTEM_LIBS}
server.o : ${SOURCES_SERVER}
	${CC} ${CFLAGS} -c ${SOURCES_SERVER}

client : client.o
	${CC} ${CFLAGS} -DSIMULATE_DROP=1 -o client ${SOURCES_CLIENT:.c=.o} ${SYSTEM_LIBS}
client.o : ${SOURCES_CLIENT}
	${CC} ${CFLAGS} -DSIMULATE_DROP=1 -c ${SOURCES_CLIENT}

prifinfo_plus : prifinfo_plus.o
	${CC} ${CFLAGS} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${SYSTEM_LIBS}
prifinfo_plus.o : prifinfo_plus.c get_ifi_info_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c get_ifi_info_plus.c

file_printer : file_printer.o
	${CC} ${CFLAGS} -o file_printer file_printer.o ${SOURCES_UTILS:.c=.c} ${SYSTEM_LIBS}
file_printer.o : file_printer.c
	${CC} ${CFLAGS} -c file_printer.c ${SOURCES_UTILS}

.PHONY: clean
clean:
	${RM} *.o
