#include "mavlink.h"
#include "mavlink_parser.h"

void mav_heartbeat(mavlink_message_t *recvd_msg);

enum ENUM_MAV_CMDS {
    ENUM_HANDLER_FUNC(mav_heartbeat),
    MAV_CMD_CNT
};

struct mavlink_parser_item cmd_list[] = {
    MAV_CMD_DEF(mav_heartbeat, 0),
};

void parse_mavlink_received_msg(mavlink_message_t *msg)
{
    int i;
    for(i = 0; i < (signed int)CMD_LEN(cmd_list); i++) {
        if(msg->msgid == cmd_list[i].msg_id) {
            cmd_list[i].handler(msg);
            break;
        }
    }
}

void mav_heartbeat(mavlink_message_t *recvd_msg)
{
    //TODO
}
