#include "app_settings.h"

#define LOG_TAG "MsgHeader"

msg_header *
get_new_header ()
{
    msg_header *header = (msg_header *) malloc(sizeof(msg_header));

    header->sequence_num = INVALID_SEQUENCE_NUM;
    header->acknowledgment_num = INVALID_ACKNOWLEDGMENT_NUM;
    header->flags = 0;
    header->advertized_window = INVALID_ADVERTIZED_WINDOW;
    header->payload_length = -1;

    return header;
}

char *
msg_header_flags_to_string (uint16_t flags)
{
    static char flag_string[MAX_FILENAME];
    char *temp = flag_string;

    temp += sprintf(temp, "%s", "_");
    
    if (0 != (flags & MSG_HEADER_FLAG_FILENAME))
    {
        temp += sprintf(temp, "%s", "FILENAME_");
    }
    if (0 != (flags & MSG_HEADER_FLAG_SYN))
    {
        temp += sprintf(temp, "%s", "SYN_");
    }
    if (0 != (flags & MSG_HEADER_FLAG_FIM))
    {
        temp += sprintf(temp, "%s", "FIM_");
    }
    if (0 != (flags & MSG_HEADER_FLAG_ACK))
    {
        temp += sprintf(temp, "%s", "ACK_");
    }
    if (0 != (flags & MSG_HEADER_FLAG_PRB))
    {
        temp += sprintf(temp, "%s", "PRB_");
    }

    return flag_string;
}

void
print_msg_header (msg_header *header, char *prefix)
{
    LOGS("%s Seq=%u Ack=%u Flags=%s Win=%u Len=%u", prefix, header->sequence_num,
            header->acknowledgment_num, msg_header_flags_to_string(header->flags),
            header->advertized_window, header->payload_length);
}
