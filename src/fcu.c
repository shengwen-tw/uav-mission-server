/* MAVLink parser for flight control unit (FCU) */

#include <stdbool.h>
#include "common.h"
#include "config.h"
#include "mavlink.h"
#include "mavlink_parser.h"
#include "rtsp_stream.h"
#include "siyi_camera.h"
#include "util.h"

#define FCU_CHANNEL MAVLINK_COMM_1

extern bool serial_workaround_verbose;
static bool serial_status = false;

static void mav_fcu_ping(mavlink_message_t *recvd_msg)
{
    status("FCU: Received ping message.");
}

static void mav_fcu_gps_raw_int(mavlink_message_t *recvd_msg)
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

static void mav_fcu_rc_channels(mavlink_message_t *recvd_msg)
{
#define INC 0.3

    static float cam_yaw = 0;
    static float cam_pitch = 0;
    static uint16_t button_a_last = 0;
    static uint16_t button_snapshot_last = 0;
    static uint16_t record_last = 0;

    int rc_yaw_min = get_rc_config_min(1);
    int rc_yaw_mid = get_rc_config_mid(1);
    int rc_yaw_max = get_rc_config_max(1);
    bool rc_yaw_reverse = get_rc_config_reverse(1);

    int rc_pitch_min = get_rc_config_min(2);
    int rc_pitch_mid = get_rc_config_mid(2);
    int rc_pitch_max = get_rc_config_max(2);
    bool rc_pitch_reverse = get_rc_config_reverse(2);

    int rc_scroll_min = get_rc_config_min(9);
    int rc_scroll_max = get_rc_config_max(9);

    mavlink_rc_channels_t rc_channels;
    mavlink_msg_rc_channels_decode(recvd_msg, &rc_channels);

    /* Map RC signals to [-100, 100] */
    float rc_yaw = ((float) rc_channels.chan1_raw - rc_yaw_mid) /
                   (rc_yaw_max - rc_yaw_min) * 200;
    float rc_pitch = ((float) rc_channels.chan2_raw - rc_pitch_mid) /
                     (rc_pitch_max - rc_pitch_min) * 200;
    uint16_t button_a = rc_channels.chan5_raw;
    uint16_t button_snapshot = rc_channels.chan13_raw;
    uint16_t zoom = rc_channels.chan9_raw;
    uint16_t record = rc_channels.chan14_raw;

    /* Zooming */
    static int zoom_ratio = 10;  // 1.0
    static int zoom_dir = 0;
    int zoom_inc = 5;  // 0.5
    static bool zoom_stop = false;

    /* Initialization */
    if (button_a_last == 0)
        button_a_last = button_a;

    if (record_last == 0)
        record_last = record;

    if (button_snapshot_last == 0)
        button_snapshot_last = button_snapshot;

    /* Reverse directions */
    rc_yaw *= rc_yaw_reverse ? -1 : +1;
    rc_pitch *= rc_pitch_reverse ? -1 : +1;

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
        rtsp_stream_save_image(0);
    }

    /* Handle zoom button */
    if (zoom <= rc_scroll_min) {
        zoom_stop = true;
        zoom_dir = +1;
    } else if (zoom >= rc_scroll_max) {
        zoom_stop = true;
        zoom_dir = -1;
    } else if (zoom_stop) {
        zoom_stop = false;
        zoom_ratio += zoom_dir * zoom_inc;

        if (zoom_ratio < 10) {
            zoom_ratio = 10;
        } else if (zoom_ratio > 40) {
            zoom_ratio = 40;
        }

        printf("Zoom ratio: %d.%d\n", zoom_ratio / 10, zoom_ratio % 10);

        siyi_cam_manual_zoom(zoom_ratio / 10, zoom_ratio % 10);
    }

    /* Handle video recording button */
    if (record != record_last) {
        record_last = record;
        rtsp_stream_change_recording_state(0);
    }

    /* Send camera control signal */
    siyi_cam_gimbal_rotate((int16_t) (cam_yaw * 10),
                           (int16_t) (cam_pitch * 10));
}

static void mav_fcu_autopilot_version(mavlink_message_t *recvd_msg)
{
    serial_status = true;

    status("FCU: received autopilot version message.");
}

static struct mavlink_cmd fcu_cmds[] = {
    DEF_MAVLINK_CMD(mav_fcu_ping, 4),
    DEF_MAVLINK_CMD(mav_fcu_gps_raw_int, 24),
    DEF_MAVLINK_CMD(mav_fcu_rc_channels, 65),
    DEF_MAVLINK_CMD(mav_fcu_autopilot_version, 148),
};

static mavlink_status_t fcu_status;
static mavlink_message_t fcu_msg;

static bool fcu_verbose = false;

void fcu_read_mavlink_msg(uint8_t *buf, size_t nbytes)
{
    const size_t msg_cnt = sizeof(fcu_cmds) / sizeof(struct mavlink_cmd);
    for (int i = 0; i < nbytes; i++) {
        if (mavlink_parse_char(FCU_CHANNEL, buf[i], &fcu_msg, &fcu_status) ==
            1) {
            parse_mavlink_msg(&fcu_msg, fcu_cmds, msg_cnt);
        }
    }

    if (fcu_verbose)
        status("FCU: Received undefined message #%d", fcu_msg.msgid);
}

bool serial_is_ready(void)
{
    return serial_status;
}
