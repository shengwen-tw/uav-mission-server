#include "mavlink_parser.h"
#include "common.h"
#include "mavlink.h"

#define SYS_ID 1 /* System ID of this program */

bool verbose = true;

void mav_heartbeat(mavlink_message_t *recvd_msg);
void mav_command_long(mavlink_message_t *recvd_msg);

enum ENUM_MAV_CMDS {
    ENUM_MAVLINK_HANDLER(mav_heartbeat),
    ENUM_MAVLINK_HANDLER(mav_command_long),
    MAV_CMD_CNT
};

struct mavlink_cmd cmd_list[] = {
    DEF_MAVLINK_CMD(mav_heartbeat, 0),
    DEF_MAVLINK_CMD(mav_command_long, 76),
};

uint8_t get_sys_id(void)
{
    return SYS_ID;
}

void parse_mavlink_msg(mavlink_message_t *msg)
{
    for (int i = 0; i < CMD_LEN(cmd_list); i++) {
        if (msg->msgid == cmd_list[i].msg_id) {
            cmd_list[i].handler(msg);
            return;
        }
    }

    if (verbose)
        printf("[INFO] received undefined message #%d\n", msg->msgid);
}

void mav_heartbeat(mavlink_message_t *recvd_msg)
{
    if (verbose)
        printf("[INFO] received heartbeat message.\n");
}

void mav_command_long(mavlink_message_t *recvd_msg)
{
    if (verbose)
        printf("[INFO] received command_long message.\n");

    /* Decode command_long message */
    mavlink_command_long_t mav_cmd_long;
    mavlink_msg_command_long_decode(recvd_msg, &mav_cmd_long);

    /* Ignore the message if the target id does not match the system id */
    if (get_sys_id() != mav_cmd_long.target_system) {
        return;
    }

    switch (mav_cmd_long.command) {
    case MAV_CMD_DO_SET_ROI_LOCATION: /* 195 */
        // mav_cmd_do_set_roi_location_handler(&mav_cmd_long);
        break;
    case MAV_CMD_DO_SET_ROI_NONE: /* 197 */
        // mav_cmd_do_set_roi_none_handler(&mav_cmd_long);
        break;
    case MAV_CMD_REQUEST_MESSAGE: /* 512 */
        // mav_cmd_request_msg_handler(&mav_cmd_long);
        break;
    }
}
