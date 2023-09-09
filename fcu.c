/* MAVLink parser for flight control unit (FCU) */

#include <stdbool.h>
#include "mavlink.h"
#include "mavlink_parser.h"
#include "util.h"

#define FCU_CHANNEL MAVLINK_COMM_1

void mav_fcu_gps_raw_int(mavlink_message_t *recvd_msg);
void mav_fcu_rc_channels(mavlink_message_t *recvd_msg);
void mav_fcu_autopilot_version(mavlink_message_t *recvd_msg);

extern bool serial_workaround_verbose;

/* clang-format off */
enum {
    ENUM_MAVLINK_HANDLER(mav_fcu_rc_channels),
    ENUM_MAVLINK_HANDLER(mav_fcu_gps_raw_int),
    ENUM_MAVLINK_HANDLER(mav_fcu_autopilot_version),
    FCU_MAV_CMD_CNT
};
/* clang-format on */

struct mavlink_cmd fcu_cmds[] = {
    DEF_MAVLINK_CMD(mav_fcu_gps_raw_int, 24),
    DEF_MAVLINK_CMD(mav_fcu_rc_channels, 65),
    DEF_MAVLINK_CMD(mav_fcu_autopilot_version, 148)};
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

    if (fcu_verbose)
        status("FCU: Received gps_raw_int message.");
}

void mav_fcu_rc_channels(mavlink_message_t *recvd_msg)
{
    mavlink_rc_channels_raw_t rc_channels_raw;
    mavlink_msg_rc_channels_raw_decode(recvd_msg, &rc_channels_raw);

    /* Check: mavlink_msg_rc_channels_raw.h
    typedef struct __mavlink_rc_channels_raw_t {
        uint32_t time_boot_ms;
        uint16_t chan1_raw;
        uint16_t chan2_raw;
        uint16_t chan3_raw;
        uint16_t chan4_raw;
        uint16_t chan5_raw;
        uint16_t chan6_raw;
        uint16_t chan7_raw;
        uint16_t chan8_raw;
        uint8_t port;
        uint8_t rssi;
    } mavlink_rc_channels_raw_t;
    */

    if (fcu_verbose)
        status("FCU: Received rc_channels message.");
}

void mav_fcu_autopilot_version(mavlink_message_t *recvd_msg)
{
    serial_status = true;

    if (serial_workaround_verbose)
        status("FCU: received autopilot version message.");
}

bool serial_is_ready(void)
{
    return serial_status;
}
