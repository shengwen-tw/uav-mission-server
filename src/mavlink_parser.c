#include "mavlink_parser.h"
#include "common.h"
#include "mavlink.h"

void parse_mavlink_msg(mavlink_message_t *msg,
                       struct mavlink_cmd *cmd_list,
                       size_t msg_cnt)
{
    for (int i = 0; i < msg_cnt; i++) {
        if (msg->msgid == cmd_list[i].msg_id) {
            cmd_list[i].handler(msg);
            return;
        }
    }
}
