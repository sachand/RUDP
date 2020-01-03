-----------------------------------------------------------
CSE533 Network Programming, Assignment2
Author: Saksham Chand, 109954969 <schand@cs.stonybrook.edu>
-----------------------------------------------------------



----------------------------
PLEASE NOTE BEFORE EXECUTION
----------------------------

1. Wherever mentioned, code pointers are given in the form
        <element_name>#<file_name>

2. The client opens an XTERM window to spew the contents of the file
    it received. Please launch X-Server on client node. Once file
    transfer is complete, the window will stay alive for seconds
    mentioned in XTERM_SLEEP_AFTER_COMPLETE_S#app_settings.h.
    It can be terminated before that or even during the file
    transfer w/o any side-effects. A note of caution is that in the
    compserv and sbpub nodes, the scroll history size of xterms is
    quite low. If the grader thinks it is causing trouble in
    validation, a small code modification will help. In function
    do_relay#file_printer.c, change the parameter 'destination'
    to any local file. Upon completion, that stored file may be
    used for validation.

3. The logs that you see on screen take the following prefix:

<Time of log> <Type of Log> <File identifier> <Process Id> <Thread Id>
  HH:MM:SS     S/E/I/W/D/V       LOG_TAG         xxxxx        xxxxx

This prefix is followed by the log itself.
In Solaris, gettid() is not defined. So, for Solaris, this code
prints pthread_self() which is a number ususally starting from 1 and
is not tied to the actual thread id. This number is however still
unique at any point of time and is still helpful.

4. There is a section regarding reading the logs of the application at
the end.



-----------------
TABLE OF CONTENTS
-----------------

* Protocol Header......................................................1
* Server Hub...........................................................2
    * Server Hub Creation..............................................2
    * Server Hub Service...............................................3
* Client init..........................................................3
* Protocol API for message transfer....................................4
* Protocol Handshake...................................................4
    * Handshake Aftermath..............................................6
* Channel Implementation...............................................7
* Channel Management..................................................11
* Simulating Drops....................................................11
* Receive Window Management...........................................12
* Send Window Management..............................................12
    * Producer........................................................12
    * Consumer........................................................13
    * Retransmitter...................................................13
        * The Retransmission Timer Object.............................13
        * Congestion State............................................14
        * Zero Window Probing.........................................15
* Connection Termination..............................................15
* Reading Logs........................................................16
* References..........................................................18



                                                                [Page 1]
---------------
PROTOCOL HEADER
---------------
The protocol header is influenced by TCP's header.

Protocol Header Format: (Even the figure is influenced by RFC793.)

    0                   1                   2                   3   
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Sequence Number                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                    Acknowledgment Number                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |P|F|A|S|F|                               |
    |        Unused       |R|I|C|Y|I|            Window             |
    |                     |B|M|K|N|L|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Payload Length                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Sequence Number:   32 bits
    Sequence number of the message contained in this segment except
    when SYN control bit is set. If SYN control bit is set,
    sequence number is ISN.

Acknowledgment Number:   32 bits
    If the ACK control bit is set this field contains the value of the
    next sequence number the sender of the segment is expecting to
    receive. If ACK control bit is not set, this value is garbage.

Unused:   11 bits
    Unused. Can be anything. Isn't processed by the protocol.
    
Control Bits:   5 bits
    FIL:    This segment is the first that the client sends to the
            server. The desired file name is given in the segment.
    SYN:    Synchronize sequence numbers
    ACK:    Acknowledgment field significant
    FIM:    No more data from sender
    PRB:    This is an empty probe message.

Window:   16 bits
    The number of messages beginning with the one indicated in the
    acknowledgment field which the sender of this segment is willing to
    accept.

Payload Length:   32 bits
    The number of bytes in the payload of this segment.

Note that this protocol doesn't employ any checksumming as it isn't
really required. UDP (and all layers underneath) shall take care of
that.
    CODE: msg_header.c, msg_header.h



                                                                [Page 2]
-------------------
SERVER HUB CREATION
-------------------
Server hub is the name given to the module that is responsible for:
    
    1. Listing out all interfaces.
    2. Bind to all appropriate interfaces.
    3. Wait for incoming requests from clients.

Point 1 is acheived with the help of get_ifi_info_plus function.
Idea is to use this list to create a new list of 'endpoints' that are
up excluding wildcard and recording their unicast address information.
Structure used for same is named 'endpoint':

    /**
     * Stores information of all appropriate interfaces
     * */
    typedef struct endpoint_t
    {
        /**
         * Socket with which the hub is bound to that interface
         * */
        SOCKET sock;

        /**
         * Unicast n/w addr of the endpoint and subnet address info.
         * */
        struct sockaddr_in network_address;
        struct sockaddr_in subnet_mask;

        /**
         * A human readable description of this structure.
         * */
        char desc[128];

        struct endpoint_t *next;
    } endpoint;

To find appropriate interfaces, the code enumerates over each ifi_info.

    o First it rejects an ifi_info if it is not up
    o Then it rejects if ifi_addr is NULL
    o Then it rejects if ifi_addr is wildcard.
    
An address passing all above is stored in our endpoint list and the
code binds a socket with it on the port number specified in the
server.in file. Addresses are stored in the priority of their subnet
masks. Higher masks have higher priority and are therefore earlier in
list. This covers Point 2.
    CODE: create_listen_hub#server_hub_creator.c
    CODE: get_unicast_endpoint_list#endpoint.c

Point 3 is a simple select on all the above bound sockets.
    CODE: manage_hub#server_hub_manager.c



                                                                [Page 3]
------------------
SERVER HUB SERVICE
------------------
The created hub provides file transfer service to its clients. When a
client approaches the server, it checks if this client is already being
serviced. And by this client, we mean the client's IP/Port pair.For
each new request, the hub creates a 'worker' to process the request:

    /**
     * Identifier for each worker.
     * This stands for the remote endpoint
     * */
    typedef struct worker_id_t
    {
        uint32_t remote_ip;
        uint16_t remote_port;
    } worker_id;

    /**
     * Details of a worker
     * */
    typedef struct worker_t
    {
        worker_id id;
        pid_t tag;

        struct worker_t *next;
    } worker;

This information is stored in a global list which is deleted once the
program terminates.
    CODE: handle_incoming_request#server_hub_manager.c
    CODE: server_workers.c, server_workers.h

-----------
CLIENT INIT
-----------
The client reads server IP from the .in file and finds the best possible
interface that it should use to connect with the server. Mechanism:

    1. Check if self and server are same. If so, go on loopback. This
        can be verified from logs. Look out for statement:
        "Server intended to be on same host. Going on LOOPBACK".
        
    2. Go through endpoint list, which is sorted in decreasing order
        of subnet masks and check if server is local with some
        interface. If so, select it. This can be verified from logs with
        "Server found on same subnet. Enabling DONTROUTE".
        
    3. endpoint list has exhausted. Pick any interface and connect using
        it. This can be verified from logs. Look out for statement:
        "Server not on the same subnet. Binding with arbitrary interface:"
        followed by which interface was chosen.

In steps 1 and 2, SO_DONTROUTE is set.
    CODE: client_connect#client_server_connector.c



                                                                [Page 4]
---------------------------------
PROTOCOL API FOR MESSAGE TRANSFER
---------------------------------
Applications interact with the RUDP channel using two functions:
messages with payload are sent out through 'send_rudp' and received
through 'recv_rudp' functions.
        CODE: channel_wrapper.c

------------------
PROTOCOL HANDSHAKE
------------------
A handshake similar to TCP's:

      CLIENT                                    SERVER
        |                                          |
        |-------------[FIL_FILENAME_]------------->|
        |                                          |
        |<----------------[SYN_X_]-----------------|
        |                                          |
        |-------------[SYN_ACK_Y,X+1_]------------>|
        |                                          |

In the figures given in this section, blank fields indicate don't care
fields. They are not processed by the protocol.

[FIL_FILENAME]:
    This is the first message relayed and is initiated by client.
    In this message, 
        
        1. The FIL control bit is set.
        2. The filename is transferred as payload.
        3. Payload length field contains the length of file name.
        4. All other fields are Don't-Care.

    Diagrammatically,
    
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |0|0|0|0|F|                               |
    |                     |0|0|0|0|I|                               |
    |                     |0|0|0|0|L|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       Filename length                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                           Filename                            |
    ...                                                           ...



                                                                [Page 5]

[SYN_X]:
    In response to this, server first checks for the file in the system.
    If found server responds with a SYN as described by the following
    figure:
        
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                            ISN_X                              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |0|0|0|S|0|                               |
    |                     |0|0|0|Y|0|        Server's rwnd          |
    |                     |0|0|0|N|0|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      Port Number length                       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       New port number                         |
    ...                                                           ...
        
    SYN packet is set and ISN given(denoted by X). Server also supplies
    its rwnd length. However note that in our file transfer case,
    server's rwnd will never be used. Note that this message will be
    forwarded by both the server parent and the server child.
    
    If the file is NOT found, server(parent only) responds with FIM_ACK:
    
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |0|F|A|0|0|                               |
    |                     |0|I|C|0|0|                               |
    |                     |0|M|K|0|0|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    
    Please note that unlike other control messages, this message is sent
    just once and the connection is immediately closed after that.
    The reason for doing this and not a 2/3/4-way termination
    handshake is because those will be overkill for this case.
    
    If the FIM_ACK message is lost in transit, the client will timeout
    waiting for a reply from the server and will send the FIL message
    again. The server will start the connecting process again, it will
    again find out the file is not present and will send FIM_ACK again.
    This goes on either till client gets the FIM_ACK or client gives up
    on the connection.



                                                                [Page 6]
[SYN_ACK_Y,X+1_]:
    If the response that the client gets from the above is FIM_ACK, the
    client quits. Otherwise, the client reads the new port number and
    re-connects the socket with the new server port and sends the
    SYN_ACK on it.

    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                            ISN_Y                              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                           ACK_X+1                             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |0|0|A|S|0|                               |
    |                     |0|0|C|Y|0|        Client's rwnd          |
    |                     |0|0|K|N|0|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |000000000000000000000000000000000000000000000000000000000000000|
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    In this message,
    
        1. The Client sets SYN and ACK control bits.
        2. Client writes its ISN(denoted by Y)
        3. Client responds to the server's previous SYN, by ACK-ing X+1
        4. Client sends its rwnd that it read from client.in.


    It is important to note that when server child receives this message
    it shall unconnect the listening socket that it got from server
    parent and then close it. This is vital to the concurrency of the
    server.

    CODE: client_do_handshake#client_handshake.c
    CODE: server_do_handshake#server_handshake.c
    CODE: ctl_message_handler.c <--- This has the send/recv part

This completes the handshake. It should also be noted that any of these
messages can be lost in transit by simulating drop at client

-------------------
HANDSHAKE AFTERMATH
-------------------
After a successful handshake, both parties would have made their channel
counterpart.
    CODE: channel_provider.h, channel_provider.c

SYN_preprocess: Before sending out a SYN, each party makes its recv
    window and generates a random ISN to send out. The window parameter
    acknowledged in the SYN messages is derived from here.
SYN_postprocess: After receiving a SYN, each party makes its sending
    window based on the size it receives from the other party for its
    rwnd.




                                                                [Page 7]
----------------------
CHANNEL IMPLEMENTATION
----------------------
After a connection is established ecah party essentially creates a
channel. A channel is defined as:

    /**
     * The channel.
     * Each channel has two parts - the send and the recv channel.
     * */
    typedef struct channel_t
    {
        channel_id id;
        send_channel sink;
        recv_channel source;
        SOCKET sock;
        uint32_t next_seq_num_send;
        uint32_t last_seq_num_recv;

        struct channel_t *next;
    } channel;

1. channel_id is the component which identifies a channel:

    /**
     * Channel id.
     * Each channel is recognized by its endpoints pair.
     * */
    typedef struct channel_id_t
    {
        uint32_t local_ip;
        uint16_t local_port;
        uint32_t remote_ip;
        uint16_t remote_port;
    } channel_id;

2. sink is the send_channel of this channel. sink takes care of the
sending part of this channel. It is defined by the following structs.
They are explained further in the document.

    /**
     * Encapsulates the send channel itself
     * */
    typedef struct send_channel_t
    {
        /**
         * The circular queue that stores data
         * */
        queue window;



                                                                [Page 8]
        /**
         * State information of the send channel
         * */
        send_channel_state state;

        /**
         * We want restricted access to this channel.
         * access_mutex to guard the channel.
         * 
         * NOTE: Through this mutex, we guard the window.
         * window on itself is unguarded.
         * */
        pthread_mutex_t access_mutex;

        /**
         * Signal that tells if window was just unlocked
         * */
        pthread_cond_t window_unlocked;

        /**
         * Stores duplicate ack count for the oldest member in the
         * window
         * */
        int duplicate_ack_count;

        /**
         * Whether the channel is active or not. Inactive channels are
         * either babies(un-initialized ones) or oldies(dying ones).
         * */
        boolean active;
    } send_channel;

    /**
     * Encapsulates information about the send_channel
     * */
    typedef struct send_channel_state_t
    {
        //------------------------------------------------------------
        // Congestion control
        /**
         * CONGESTION WINDOW (cwnd): A TCP state variable that limits
         * the amount of data a TCP can send.  At any given time, a
         * TCP MUST NOT send data with a sequence number higher than the
         * sum of the highest acknowledged sequence number and the
         * minimum of cwnd and rwnd.
         * */
        int cwnd;

        /**
         * SLOW START THRESHOLD (ssthresh): The slow start threshold is
         * used to determine whether the slow start or congestion
         * avoidance algorithm is used to control data transmission.
         * */
        int ssthresh;



                                                                [Page 9]
        /**
         * ACKNOWLEDGMENTS PER RTT (ack_per_round_trip): This is the
         * number of acks that we recieved per RTT. This is only used
         * in congestion avoidance or AI mode.
         * */
        int ack_per_round_trip;

        //--------------------------------------------------------------
        // Retransmission control
        /**
         * State to represent current retransmission engine
         * */
        retransmission_state retxmt_info;
        
        /**
         * retransmitter is the thread that actually retransmits data.
         * */
        pthread_t retransmitter;
        
        /**
         * stop_retransmission_alarm is the condition that retransmitter
         * waits on. When a message is transmitted, retransmitter, waits
         * on this condition for rto timeout. If meanwhile, this
         * condition is signalled, retransmitter is signalled which
         * makes it break out of its sleep.
         * */
        pthread_cond_t stop_retransmission_alarm;

        /**
         * transmitted_something essentially signals the retransmitter
         * that something was just transmitted, so maybe retransmitter
         * should watch that.
         * */
        sem_t transmitted_something;

        /**
         * Dynamically changing size of the receive window of other party
         * */
        int rwnd;
    } send_channel_state;

3. source refers to the recv_channel of this channel. This is where
data received over the socket from other party is buffered.

    /**
     * Encapsulates recv channel itself
     * */
    typedef struct recv_channel_t
    {
        /**
         * The circular queue that stores data
         * */
        queue window;



                                                               [Page 10]
        /**
         * We want restricted access to this channel.
         * access_mutex to guard the channel.
         * 
         * NOTE: Through this mutex, we guard the window.
         * window on itself is unguarded.
         * */
        pthread_mutex_t access_mutex;

        /**
         * Whether the channel is active or not. Inactive channels are
         * either babies(un-initialized ones) or oldies(dying ones).
         * */
        boolean active;
    } recv_channel;

4. sock is the socket this channel monitors

5. next_seq_num_send refers to the sequence number that the send channel
uses while pushing next segment in buffer

6. last_seq_num_recv is the variable used by recv_channel and refers to
the last *IN ORDER* sequence number that this channel has received.
Please note *IN ORDER* again and again and again... *IN ORDER*.
Can't stress enough, but I have to because I chose an incorrect name :/


Last thing to note in this section is probably the queue data type.
It is a simple array of buffer headers. Each element of the queue's
array is of type: (Usage in further sections)

    /**
     * Construct to point to the data part of a queue cell
     * */
    typedef struct queue_element_t
    {
        msg_iovec msg;
        uint64_t timestamp;
        uint8_t transmission_count;
    } queue_element;

The queue is defined as:

    /**
     * Construct to represent a queue itself.
     * Implemented as an array simulating a queue.
     * */
    typedef struct queue_t
    {
        int size;
        int head;
        int tail;

        boolean initialized;

        queue_element_list elements;
    } queue;



                                                               [Page 11]
The queue maintains two indices on its array called tail and head.
    tail: The oldest message in the queue
    head: Index for next segment to enter

------------------
CHANNEL MANAGEMENT
------------------
Now we talk about how the code handles the sending/receiving of messages
from the window. Note that the control messages described above and in
the rest of the paper do not go through the window, otherwise we'll end
up ACKing ACKs!

A point where this code stands apart from Stevens' implementation is its
dismissal of the signal-based retransmission. The signal-siglongjmp method
is replaced by a retransmission thread that uses pthread_cond_timedwait
instead. The reason for this is that siglongjmp is a form of goto and
the author of this code is a strong radical supporter of Dijkstra's
seminal letter, "Go To Statement Considered Harmful"[6]

Moving on, I first present the recv window logic as it is easier and
finally the send window logic.

----------------
SIMULATING DROPS
----------------
Drops are simulated at the client side for both sends and recvs on the
socket. To simulate drop, the following code is used:

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

This snippet resides in client.c. Parameter 'g_dg_loss_probability'
is read from client.in.

We describe its use by explaining the functions that invoke it:

1. Recv:
Before reading anything from the socket, the client (and the server)
does a timedout-based-select on the socket. This select is
where we invoke the drop. We wrap this select with a function
named ready_socket_count.

The logic is to first select for given timeout. If nothing arrived during
that time, function responds as such. Otherwise, it checks for should_drop().
If it is instructed to drop, this function performs a recv on the socket
effectively consuming it. Then it calcuates how much time is left from
the originally supplied timeout. It then iterates over this value.
    CODE: ready_socket_count#socket_common.c
    CODE: socket_recv_msg_default#socket_common.c

P.S.: The code avoids the 'Race Conditions' mentioned in Stevens using
non-blocking select's and recv's and checking for a deciding parameter
of iteration.



                                                               [Page 12]
2. Send
Sending is much simpler. Before sending packet on socket, if should_drop
instructs to drop, then the function doesn't actually write anything
on the socket but returns the length of the msg that was supposed to be
passed. This gives the illusion to upper layers that data was in fact passed.
    CODE: socket_send_msg_default#socket_common.c

-------------------------
RECEIVE WINDOW MANAGEMENT
-------------------------
The recv window is governed by two threads - the producer and the
consumer thread. The producers task is to read incoming messages from
the socket, simulate drops and then feed the 'undropped' packets
into the window if possible. Two points are to be focussed here:

1. Message admission control
This point applies only to the 'undropped' messages.
The producer can not just blindly put any message in the window.
Insertion in the window is governed by the LAST_ORDERED_SEQUENCE_NUMBER
seen by the window. A message with sequence number less than this value or
greater than the sum of this value and the total window size is rejected.
But note that an ACK with one more than the LAST_ORDERED_SEQUENCE_NUMBER
is sent regardless.
    CODE: recv_channel_handler.c

2. Unordered but in-range packets. What to do with them???
This point applies to messages that have cleared point 2.
This implementation keeps out of order messages that pass point 2.
This means if the LAST_ORDERED_SEQUENCE_NUMBER seen is N and the window
size is M, the window has space for N+1, N+2... N+M sequence number msgs.
If the producer sees one of these, may be out of order, it will put it
in the window. However, this creates 'holes' in the window. But this
reduces the load on network by buffering whatever is possible.
    CODE: queue.c, recv_channel_handler.c

----------------------
SEND WINDOW MANAGEMENT
----------------------
Send window is more complicated because of the additional retransmitter
thread but fortunately a simpler producer + consumer.

*** PRODUCER:
Producer is the thread responsible for filling data into the window.
In sender's case it is the one controlled by the 'application'
that reads contents of a file and passes to internal layers for processing.
    CODE: file_sender.c, channel_wrapper.c

The application is free to give any sized char buffers to the underlying
protocol. As long as the size is possible to be fit in the max-sized
window, the buffer is catered, otherwise channel_wrapper throws EMSGSIZE.



                                                               [Page 13]
Internally channel_wrapper breaks down a big buffer (bigger than PAYLOAD_SIZE)
into smaller chunks of PAYLOAD_SIZE and *blocks* till these buffers are
queued in the window.

Additionally, when the producer puts a buffer in the window, it performs the
first socket-transmit attempt of the buffer. Also, note that this like
all socket-transmit attempts are non-blocking.

To summarize, producer blocks till its entire buffer is copied into
the send window not till it reaches the other end and the channel receives
an ACK.
    CODE: do_produce#send_channel_handler.c

*** CONSUMER:
Consmuer's responsibility is to clean stuff up. When an ACK is received,
the consumer removes all messages from the queue with sequence numbers
less that the ACK number. This also signals the Producer that some
space has just been created. If the Producer was waiting for this
signal, it unblocks and adds buffers in the window.

Additionally, this thread performs the congestion related calculations
which apply to it.
    CODE: process_ack#send_channel_handler.c

*** RETRANSMITTER:
Retransmitter is the third and final thread in the send framework.
Because it is too involved, I'll break it up into parts and build from
scratch.

1. The Retransmission Timer object
Each send channel needs to retransmit and therfore keeps track of a
'retransmission_state'. Each state is defined as the following collection:

    /**
     * Holder of necessary retransmission info
     * */
    typedef struct retransmission_state_t
    {
        /**
         * Current RTO
         * */
        int rto_ms;
        
        /**
         * Scaled up value of Smoothed RTT
         * */
        int srtt_scaled;

        /**
         * Scaled up value of RTT variance
         * */
        int rttvar_scaled;

        pthread_mutex_t synchronizer;
    } retransmission_state;



                                                               [Page 14]
Details about the implementation are in the files mentioned in this
subsection's CODE pointers. The code uses integer bit shift arithmetic
to carry out the operations. The key differences from Stevens' code would be

    o Use of integer arithmetic instead of floating-point
    o Offering millisecond granularity
    o Storing scaled srtt and rttvar instead of real
    o MIN, MAX, RETXMT values are changed to 1000 ms, 3000 ms and 12
        respectively.

    CODE: retransmission_timer_6298.c, retransmission_timer_6298.h
            [for points 1..3]
    CODE: app_settings.h [for point 4]

Also, this code uses Karn's suggestion and does not use ACKs for segments
transmitted more than once.

2. Congestion state
Each send_channel also maintains a congestion state by means of two
variables - cwnd and ssthresh.

The intial values for cwnd and ssthresh are 1 and receiver's rwnd
respectively.

During the course of operation, these values are updated at
each ACK or during Fast retransmit or during Retransmission timeout.
Please note that the code does not grow cwnd boundlessly. Value of
cwnd is bounded by the size of the initial window i.e., the rwnd that
the receiver sent during handshake. This is an appropriate assumption
and is reasoned in the document. The details are documented well in the
code pointer below.
    CODE: congestion_control_5681_2581.c

However, I document how the control is actually performed. The code largely
follows RFC 5681 except for the case of Fast Recovery. Congestion control
is performed upon receipt of ACKs or during a retransmission timeout.

    o In case the sender notices a retransmission timeout, the cwnd is
    dropped invariably to 1 while the ssthresh is reset to
    max (FlightSize / 2, 2).
    
    o In case of three Duplicate ACKs and therefore Fast Recovery,
    the protocol retransmits the desired message immediately, restarts the
    retransmitter, sets ssthresh to max (FlightSize / 2, 2) and sets cwnd
    to this new value of ssthresh.
    
    o In case of any other ACK, the code compares cwnd and ssthresh.
    If cwnd < ssthresh ==> MI mode or the Slow Start mode
        - In this mode, for each ACK, we increase the cwnd by 1.
    Otherwise ==> AI mode or Congestion Avoidance mode
        - Here, we increase cwnd by 1 per Round trip i.e., we accumulate
        cwnd number of ACKs. If we reach those many, we increase
        cwnd by 1.



                                                               [Page 15]
3. Probing
Probing is the technique employed upon zero window advertisement. This
method is advised in RFC 1122 under heading 'Probing Zero Windows'.
The implementation that this code exercises is influenced by a discussion
in this segment of the RFC:

    """
    This procedure minimizes delay if the zero-window
    condition is due to a lost ACK segment containing a
    window-opening update.  Exponential backoff is
    recommended, possibly with some maximum interval not
    specified here.  This procedure is similar to that of
    the retransmission algorithm, and it may be possible to
    combine the two procedures in the implementation.
    """

    1. The code imposes a max limit on Probe timeout. It is given as
    PROBE_MAX_TIMEOUT_MS#app_settings.h

    2. The last line in the discussion is important. It very intuitively
    says that probing and retransmission can be handled together by a
    single entity and that is what this code does. The same retransmitter
    thread is responsible for probing. Decision for probing is carried
    out by checking the 'rwnd' value of the 'send_channel'.
    Mathematically, Probing <==> (rwnd == 0).
    Thus, there is *NO* _extra_ persist timer.

    CODE: retransmitter_prober.c

----------------------
CONNECTION TERMINATION
----------------------
The final task at hand is *clean* termination. This is carried out by a
2-way handshake between server and client. When the file reaches EOF,
the producer, indicates this to the channel by sending FIM control bit set
in the arguments to the send_rudp function. This triggers a FIM_ACK
message from the sending server:

      CLIENT                                    SERVER
        |                                          |
        |-----------------[FIM_ACK_]-------------->|
        |                                          |
        |<----------------[FIM_ACK]----------------|

Server -> Client:

    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Sequence Number                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                    Acknowledgment Number                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |0|F|A|0|0|                               |
    |                     |0|I|C|0|0|                               |
    |                     |0|M|K|0|0|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Payload Length                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         Last Buffer                           |
    ...                                                           ...



                                                               [Page 16]
Client -> Server

In the ideal case, on receiving this message, client will reply with a
similar message:

    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Sequence Number'                       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         Acknowledgment Number = Sequence Number + 1           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     |0|F|A|0|0|                               |
    |                     |0|I|C|0|0|                               |
    |                     |0|M|K|0|0|                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |000000000000000000000000000000000000000000000000000000000000000|
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

When the server receives this FIM_ACK from client, it will close the
connection and begin destroying its channel.

However, things are not ideal in real world. Take the case where the client
is waiting for some earlier packet to arrive but FIM_ACK reaches the client
first. Let us assume client has last ACKed 56 and it received a FIM_ACK
with sequence number 60. In this case, the client will simply ignore this
'out-of-order' FIM_ACK and re-send ACK 56 instructing the server to send
that first. In such a case, server cannot close the connection.
This is why the FIM_ACK from client is necessary.

The functionality of the protocol is no different for the above case from
the point of view of sending and receiving messages. The same methodology
described in sections above will govern the next course of action for
this step. For e.g., in above case, when server receives ACK 56, it
cleans everything till 55, Retransmitter will take care of retransmitting
56 and so on... The FIM_ACK message is lying at the end of send window
which will be taken care of eventually by the retransmitter if un-ACKed.

------------
READING LOGS
------------

1. Server hub creation and management.
    
    The server upon starting prints .in file contents on the screen. It
    will then list all interfaces that get_ifi_info_plus gives it.
        grep "Analyzing" for that.
    
    As server binds to the sockets, it logs the event.
        grep "Binding socket on" for that.

    To check whether SO_DONTROUTE was set or not,
        grep "dont route" for that.



                                                               [Page 17]
2. Client.

    The client upon starting prints the .in file contents on the screen.
    It, like the server prints all interfaces that get_ifi_info_plus
    gives it.
        grep "Analyzing" for that.
    
    Client prints whether server was on same subnet and whether DONTROUTE
    was set.
        grep "ClientConnect" for that.

3. Socket SEND and RECV messages.

    When a message is given for the core socket handlers to operate on,
    the message is either sent, received or is dropped. All of these
    scenarios get logged.
        grep "SEND" to know what messages were sent
        grep "RECV" to know what messages were received
        grep "Dropping" to know what messages were dropped
    
    The first two of the above print the entire message header. The
    last one just prints the sequence number and flags contained in the
    header. Note that NULL or EMPTY flags get printed as "_".

4. Retransmitter messages.

    Under normal conditions, two of its logs are important:

    o When the timer expires, it prints what sequence number message
    is being retransmitted andits retransmit count.
        grep "Retxmtting" for that.
    o For probing,
        grep "PRB"

5. Congestion control messages.

    These are of two kinds and for all,
        grep "Updated cwnd" for them

    o First happens due to ACKs. In them the log also tells what was the
    algorithm state before the update - SS for Slow Start and AI
    for Additive increase.
    
    o For other cases, the log prints the reason - whether Fast Recovery
    or Transmit Timeout.

6. By default, the application will not print when it read or sent
certain number of bytes to the underlying protocol. If you want that,
lower the log_level.
    - Change the global g_log_level from INFO to DEBUG in logger.c
    (Line#11 when this README was written).



                                                               [Page 18]
----------
REFERENCES
----------

1. RFC 793, TRANSMISSION CONTROL PROTOCOL
    <https://www.ietf.org/rfc/rfc793.txt>

2. RFC 1122, Requirements for Internet Hosts -- Communication Layers
    <https://tools.ietf.org/html/rfc1122>

3. RFC 5681, TCP Congestion Control
   RFC 2581, TCP Congestion Control
   RFC 2001, TCP Slow Start, Congestion Avoidance, Fast Retransmit, and Fast Recovery Algorithms
    <http://tools.ietf.org/html/rfc5681>
    <http://tools.ietf.org/html/rfc2581>
    <http://tools.ietf.org/html/rfc2001>

4. RFC 6298, Computing TCP's Retransmission Timer
    <http://tools.ietf.org/html/rfc6298>

5. Stevens' TCP/IP Illustrated, Volume 1.

6. Edger Dijkstra, "Go To Considered Harmful"
https://files.ifi.uzh.ch/rerg/arvo/courses/kvse/uebungen/Dijkstra_Goto.pdf
