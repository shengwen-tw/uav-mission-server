/* MAVLink parser for ground control station (GCS) */

#include "mavlink.h"
#include "mavlink_parser.h"
#include "mavlink_publisher.h"
#include "util.h"

#define GCS_CHANNEL MAVLINK_COMM_2

int gcs_fd = 0;  // TODO: need to be assigned by the uart-server

void mav_gcs_heartbeat(mavlink_message_t *recvd_msg);
void mav_gcs_command_long(mavlink_message_t *recvd_msg);
void mav_gcs_gimbal_manager_set_manual_ctrl(mavlink_message_t *recvd_msg);

/* clang-format off */
enum {
    ENUM_MAVLINK_HANDLER(mav_gcs_heartbeat),
    ENUM_MAVLINK_HANDLER(mav_gcs_command_long),
    ENUM_MAVLINK_HANDLER(mav_gcs_gimbal_manager_set_manual_ctrl),
    GCS_MAV_CMD
};
/* clang-format on */

struct mavlink_cmd gcs_cmds[] = {
    DEF_MAVLINK_CMD(mav_gcs_heartbeat, 0),
    DEF_MAVLINK_CMD(mav_gcs_command_long, 76),
    DEF_MAVLINK_CMD(mav_gcs_gimbal_manager_set_manual_ctrl, 288),
};

mavlink_status_t gcs_status;
mavlink_message_t gcs_msg;

bool gcs_verbose = false;

void gcs_read_mavlink_msg(uint8_t *buf, size_t nbytes)
{
    for (int i = 0; i < nbytes; i++) {
        if (mavlink_parse_char(GCS_CHANNEL, buf[i], &gcs_msg, &gcs_status) ==
            1) {
            parse_mavlink_msg(&gcs_msg, gcs_cmds, GCS_MAV_CMD);
        }
    }

    if (gcs_verbose)
        status("GCS: Received undefined message #%d\n", gcs_msg.msgid);
}

void mav_gcs_heartbeat(mavlink_message_t *recvd_msg)
{
    status("GCS: Received heartbeat from system %d.\n", recvd_msg->sysid);
}

void mav_gcs_command_long(mavlink_message_t *recvd_msg)
{
    /* Decode command_long message */
    mavlink_command_long_t mav_cmd_long;
    mavlink_msg_command_long_decode(recvd_msg, &mav_cmd_long);

    /* Check: mavlink_msg_command_long.h
     * param1 - param7 depend on the received command,
     * check them on https://mavlink.io/en/messages/common.html
     *
    typedef struct __mavlink_command_long_t {
        float param1;
        float param2;
        float param3;
        float param4;
        float param5;
        float param6;
        float param7;
        uint16_t command;
        uint8_t target_system;
        uint8_t target_component;
        uint8_t confirmation;
    } mavlink_command_long_t;
    */

    if (gcs_verbose)
        status("GCS: Received command_long message. command = %d.\n",
               mav_cmd_long.command);

    switch (mav_cmd_long.command) {
    case MAV_CMD_REQUEST_MESSAGE: /* 512 */ {
        int req_msg = (int) mav_cmd_long.param1;

        if (req_msg == MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION) {
            mavlink_send_gimbal_manager_info(gcs_fd);
        } else if (req_msg == MAVLINK_MSG_ID_CAMERA_INFORMATION) {
            mavlink_send_camera_info(gcs_fd);
        }

        break;
    }
    case MAV_CMD_SET_CAMERA_FOCUS: /* 532 */ {
        /* TODO: set camera focus */

        /* Send acknowledgement to the GCS */
        mavlink_send_ack(gcs_fd, MAV_CMD_SET_CAMERA_FOCUS, MAV_RESULT_ACCEPTED,
                         100, 0);
    }
    case MAV_CMD_IMAGE_START_CAPTURE: /* 2000 */ {
        /* TODO: take a photo */

        /* Send acknowledgement to the GCS */
        mavlink_send_ack(gcs_fd, MAV_CMD_IMAGE_START_CAPTURE,
                         MAV_RESULT_ACCEPTED, 100, 0);
        break;
    }
    }
}

void mav_gcs_gimbal_manager_set_manual_ctrl(mavlink_message_t *recvd_msg)
{
    mavlink_gimbal_manager_set_manual_control_t gimbal_ctrl;
    mavlink_msg_gimbal_manager_set_manual_control_decode(recvd_msg,
                                                         &gimbal_ctrl);

    /* Check: mavlink_msg_gimbal_manager_set_manual_control.h
    typedef struct __mavlink_gimbal_manager_set_manual_control_t {
        uint32_t flags;
        float pitch;
        float yaw;
        float pitch_rate;
        float yaw_rate;
        uint8_t target_system;
        uint8_t target_component;
        uint8_t gimbal_device_id;
    } mavlink_gimbal_manager_set_manual_control_t;
    */

    if (gcs_verbose)
        status("GCS: Received gimbal_manager_set_manual_ctrl message.\n");
}
