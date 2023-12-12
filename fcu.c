/* MAVLink parser for flight control unit (FCU) */

#include <stdbool.h>
#include "common.h"
#include "mavlink.h"
#include "mavlink_parser.h"
#include "rtsp_stream.h"
#include "siyi_camera.h"
#include "util.h"

#define FCU_CHANNEL MAVLINK_COMM_1

void mav_fcu_ping(mavlink_message_t *recvd_msg);
void mav_fcu_gps_raw_int(mavlink_message_t *recvd_msg);
void mav_fcu_rc_channels(mavlink_message_t *recvd_msg);
void mav_fcu_autopilot_version(mavlink_message_t *recvd_msg);

extern bool serial_workaround_verbose;

/* clang-format off */
enum {
    ENUM_MAVLINK_HANDLER(mav_fcu_ping),
    ENUM_MAVLINK_HANDLER(mav_fcu_rc_channels),
    ENUM_MAVLINK_HANDLER(mav_fcu_gps_raw_int),
    ENUM_MAVLINK_HANDLER(mav_fcu_autopilot_version),
    FCU_MAV_CMD_CNT
};

struct mavlink_cmd fcu_cmds[] = {
    DEF_MAVLINK_CMD(mav_fcu_ping, 4),
    DEF_MAVLINK_CMD(mav_fcu_gps_raw_int, 24),
    DEF_MAVLINK_CMD(mav_fcu_rc_channels, 65),
    DEF_MAVLINK_CMD(mav_fcu_autopilot_version, 148)
};
/* clang-format on */

mavlink_status_t fcu_status;
mavlink_message_t fcu_msg;

bool fcu_verbose = false;
bool serial_status = false;

void fcu_read_mavlink_msg(uint8_t *buf, size_t nbytes)
{
    for (int i = 0; i < nbytes; i++) {
        if (mavlink_parse_char(FCU_CHANNEL, buf[i], &fcu_msg, &fcu_status) ==
            1) {
            parse_mavlink_msg(&fcu_msg, fcu_cmds, FCU_MAV_CMD_CNT);
        }
    }

    if (fcu_verbose)
        status("FCU: Received undefined message #%d", fcu_msg.msgid);
}

void mav_fcu_ping(mavlink_message_t *recvd_msg)
{
    status("FCU: Received ping message.");
}

void mav_fcu_gps_raw_int(mavlink_message_t *recvd_msg)
{
    mavlink_gps_raw_int_t gps_raw_int;
    mavlink_msg_gps_raw_int_decode(recvd_msg, &gps_raw_int);

    /* Check: mavlink_msg_gps_raw_int.h
     typedef struct __mavlink_gps_raw_int_t {
         uint64_t time_usec;
         int32_t lat;
         int32_t lon;
         int32_t alt;
         uint16_t eph;
         uint16_t epv;
         uint16_t vel;
         uint16_t cog;
         uint8_t fix_type;
         uint8_t satellites_visible;
         int32_t alt_ellipsoid;
         uint32_t h_acc;
         uint32_t v_acc;
         uint32_t vel_acc;
         uint32_t hdg_acc;
         uint16_t yaw;
     } mavlink_gps_raw_int_t;
     */

    //    status("FCU: Received gps_raw_int message.");
}

#define RC_YAW_MIN 1102
#define RC_YAW_MID 1515
#define RC_YAW_MAX 1927

#define RC_PITCH_MIN 1102
#define RC_PITCH_MID 1515
#define RC_PITCH_MAX 1927

#define RC_SCROLL_MIN 1102
#define RC_SCROLL_MID 1515
#define RC_SCROLL_MAX 1927

void mav_fcu_rc_channels(mavlink_message_t *recvd_msg)
{
#define INC 0.3

    mavlink_rc_channels_t rc_channels;
    mavlink_msg_rc_channels_decode(recvd_msg, &rc_channels);

    static float cam_yaw = 0;
    static float cam_pitch = 0;
    static uint16_t button_a_last = 0;
    static uint16_t button_snapshot_last = 0;
    static bool focus_stop = false;

    /* Map RC signals to [-100, 100] */
    float rc_yaw = ((float) rc_channels.chan1_raw - RC_YAW_MID) /
                   (RC_YAW_MAX - RC_YAW_MIN) * 200;
    float rc_pitch = ((float) rc_channels.chan2_raw - RC_PITCH_MID) /
                     (RC_PITCH_MAX - RC_PITCH_MIN) * 200;
    uint16_t button_a = rc_channels.chan5_raw;
    uint16_t button_snapshot = rc_channels.chan13_raw;
    uint16_t zoom = rc_channels.chan9_raw;

    /* Initialization */
    if (button_a_last == 0)
        button_a_last = button_a;

    if (button_snapshot_last == 0)
        button_snapshot_last = button_snapshot;

    /* Reverse directions */
    rc_yaw *= -1;
    rc_pitch *= -1;

#if 0
    printf(
        "[A]: %u, snapshot:%d, zoom: %u, cam-yaw: %f, cam-pitch: %f, rc-yaw: %f, "
        "rc-pitch: "
        "%f\n",
        button_a, button_snapshot, zoom, cam_yaw, cam_pitch, rc_yaw, rc_pitch);
#endif

    /* Increase the control signals */
    if (fabsf(rc_yaw) > 50.0f)
        cam_yaw += INC * (rc_yaw < 0 ? -1.0f : 1.0f);

    if (fabsf(rc_pitch) > 50.0f)
        cam_pitch += INC * (rc_pitch < 0 ? -1.0f : 1.0f);

    /* Bound signals in proper range */
    bound_float(&cam_yaw, +135.0, -135.0);
    bound_float(&cam_pitch, +25.0, -90.0);

    /* Detect button click */
    if (button_a != button_a_last) {
        button_a_last = button_a;
        cam_yaw = 0.0f;
        cam_pitch = 0.0f;
    }

    if (button_snapshot != button_snapshot_last) {
        button_snapshot_last = button_snapshot;
        rtsp_stream_save_image();
    }

    /* Handle zoom button */
    if (zoom <= RC_SCROLL_MIN) {
        siyi_cam_manual_zoom(0x1e, 0);
        focus_stop = true;
        printf("Zoom in\n");
    } else if (zoom >= RC_SCROLL_MAX) {
        siyi_cam_manual_zoom(0x01, 0);
        focus_stop = true;
        printf("Zoom out\n");
    } else if (focus_stop) {
        focus_stop = false;
    }

    /* Send camera control signal */
    siyi_cam_gimbal_rotate((int16_t)(cam_yaw * 10), (int16_t)(cam_pitch * 10));
}

void mav_fcu_autopilot_version(mavlink_message_t *recvd_msg)
{
    serial_status = true;

    status("FCU: received autopilot version message.");
}

bool serial_is_ready(void)
{
    return serial_status;
}
