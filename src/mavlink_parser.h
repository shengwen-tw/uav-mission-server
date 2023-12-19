#ifndef __MAV_PARSER_H__
#define __MAV_PARSER_H__

#include <stdint.h>
#include "mavlink.h"

#define DEF_MAVLINK_CMD(handler_function, id)     \
    {                                             \
        .handler = handler_function, .msg_id = id \
    }

struct mavlink_cmd {
    uint16_t msg_id;
    void (*handler)(mavlink_message_t *msg);
};

void parse_mavlink_msg(mavlink_message_t *msg,
                       struct mavlink_cmd *cmd_list,
                       size_t msg_cnt);

#endif
