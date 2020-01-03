#pragma once

#define HEADER_SIZE (sizeof(msg_header))

typedef struct iovec *msg_iovec;

/**
 * Defines a header. Headers are of the form:
 *
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                        Sequence Number                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                    Acknowledgment Number                      |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     |P|F|A|S|F|                               |
 *  |        Unused       |R|I|C|Y|I|            Window             |
 *  |                     |B|M|K|N|L|                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                        Payload Length                         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * */
typedef struct msg_header_t
{
    uint32_t sequence_num;
    uint32_t acknowledgment_num;

    uint16_t flags;
    uint16_t advertized_window;

    uint32_t payload_length;
} msg_header;

msg_header *
get_new_header ();

char *
msg_header_flags_to_string (uint16_t flags);

void
print_msg_header (msg_header *header, char *prefix);
