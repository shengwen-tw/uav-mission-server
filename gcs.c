/* MAVLink parser for ground control station (GCS) */

#include "mavlink.h"
#include "mavlink_parser.h"

#define GCS_CHANNEL MAVLINK_COMM_2

void mav_gcs_heartbeat(mavlink_message_t *recvd_msg);
void mav_gcs_command_long(mavlink_message_t *recvd_msg);
void mav_gcs_gimbal_manager_set_manual_ctrl(mavlink_message_t *recvd_msg);

/* clang-format off */
enum {
    ENUM_MAVLINK_HANDLER(mav_gcs_heartbeat),
    ENUM_MAVLINK_HANDLER(mav_gcs_command_long),
    ENUM_MAVLINK_HANDLER(mav_gcs_gimbal_manager_set_manual_ctrl),
    FCU_MAV_CMD_CNT
};
/* clang-format on */

struct mavlink_cmd gcs_cmds[] = {
    DEF_MAVLINK_CMD(mav_gcs_heartbeat, 0),
    DEF_MAVLINK_CMD(mav_gcs_command_long, 76),
    DEF_MAVLINK_CMD(mav_gcs_gimbal_manager_set_manual_ctrl, 288),
};

mavlink_status_t gcs_status;
mavlink_message_t gcs_msg;

void gcs_read_mavlink_msg(uint8_t *buf, size_t nbytes)
{
    for (int i = 0; i < nbytes; i++) {
        if (mavlink_parse_char(GCS_CHANNEL, buf[i], &gcs_msg, &gcs_status) ==
            1) {
            parse_mavlink_msg(&gcs_msg, gcs_cmds, FCU_MAV_CMD_CNT);
        }
    }
}

void mav_gcs_heartbeat(mavlink_message_t *recvd_msg)
{
    printf("Received heartbeat from system %d.\n", recvd_msg->sysid);
}

void mav_gcs_command_long(mavlink_message_t *recvd_msg)
{
    /* Decode command_long message */
    mavlink_command_long_t mav_cmd_long;
    mavlink_msg_command_long_decode(recvd_msg, &mav_cmd_long);

    switch (mav_cmd_long.command) {
    case MAV_CMD_REQUEST_MESSAGE: /* 512 */ {
        /*TODO:
         * response GIMBAL_MANAGER_INFORMATION
         * (gimbal discovery)
         */

        /* TODO:
         * reponse CAMERA_INFORMATION
         * (camera discovery)
         */
        break;
    }
    case MAV_CMD_IMAGE_START_CAPTURE: {
        /* TODO:
         * take photos and react GCS with
         * MAV_RESULT_ACCEPTED */
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
}
