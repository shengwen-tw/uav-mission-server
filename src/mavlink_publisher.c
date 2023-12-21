#include <math.h>
#include <pthread.h>
#include <unistd.h>

#include "config.h"
#include "fcu.h"
#include "mavlink.h"
#include "serial.h"
#include "util.h"

#define FCU_ID 1
#define RB5_ID 2
#define TUNE_CNT 19

extern SerialFd serial;
extern pthread_mutex_t serial_tx_mtx;

extern bool serial_workaround_verbose;

/* Source:
 * https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/tunes/tune_definition.desc#L89C44-L89C90
 */
/* clang-format off */
static const char *tune_table[TUNE_CNT] = {
    "MFT240L8 O4aO5dc O4aO5dc O4aO5dc L16dcdcdcdc", /*   0: startup tune             */
    "MBT200a8a8a8PaaaP",                            /*   1: ERROR tone               */
    "MFT200e8a8a",                                  /*   2: Notify Positive tone     */
    "MFT200e8e",                                    /*   3: Notify Neutral tone      */
    "MFT200e8c8e8c8e8c8",                           /*   4: Notify Negative tone     */
    "MNT75L1O2G",                                   /*   5: arming warning           */
    "MBNT100a8",                                    /*   6: battery warning slow     */
    "MBNT255a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8",      /*   7: battery warning fast     */
    "MFT255L4AAAL1F#",                              /*   8: gps warning slow         */
    "MFT255L4<<<BAP",                               /*   9: arming failure tune      */
    "MFT255L16agagagag",                            /*  10: parachute release        */
    "MFT100a8",                                     /*  11: single beep              */
    "MFT100L4>G#6A#6B#4",                           /*  12: home set tune            */
    "MFAGPAG",                                      /*  13: Make FS                  */
    "MNBG",                                         /*  14: format failed            */
    "MLL32CP8MB",                                   /*  15: Program PX4IO            */
    "MLL8CDE",                                      /*  16: Program PX4IO success    */
    "ML<<CP4CP4CP4CP4CP4",                          /*  17: Program PX4IO fail       */
    "MFT255a8g8f8e8c8<b8a8g4",                      /*  18: When pressing off button */
};
/* clang-format on */

static void mavlink_send_msg(const mavlink_message_t *msg, int fd)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    size_t len = mavlink_msg_to_send_buffer(buf, msg);

    pthread_mutex_lock(&serial_tx_mtx);
    serial_write(fd, buf, len);
    pthread_mutex_unlock(&serial_tx_mtx);
}

static void mavlink_send_camera_hearbeart(int fd)
{
    uint8_t sys_id = FCU_ID;
    uint8_t component_id = MAV_COMP_ID_CAMERA;
    uint8_t type = MAV_TYPE_CAMERA;
    uint8_t autopilot = MAV_AUTOPILOT_INVALID;
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
    uint8_t sys_status = MAV_STATE_STANDBY;

    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(sys_id, component_id, &msg, type, autopilot,
                               base_mode, custom_mode, sys_status);
    mavlink_send_msg(&msg, fd);
}

void mavlink_send_play_tune(int tune_num, int fd)
{
    if (tune_num >= TUNE_CNT)
        return;

    uint8_t sys_id = RB5_ID;
    uint8_t component_id = MAV_COMP_ID_ONBOARD_COMPUTER;
    uint8_t target_system = FCU_ID;
    uint8_t target_component = MAV_COMP_ID_ALL;
    const char *tune2 = "";  // extension of the first tune argument

    mavlink_message_t msg;
    mavlink_msg_play_tune_pack(sys_id, component_id, &msg, target_system,
                               target_component, tune_table[tune_num], tune2);
    mavlink_send_msg(&msg, fd);

    status("RB5: Sent play_tune message.");
}

void mavlink_send_request_autopilot_capabilities(int fd)
{
    uint8_t sys_id = RB5_ID;
    uint8_t component_id = MAV_COMP_ID_ONBOARD_COMPUTER;
    uint8_t target_system = FCU_ID;
    uint8_t target_component = MAV_COMP_ID_ALL;
    uint16_t command = MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES;
    uint8_t confirmation = 0;

    /* param1: version (1), param2-7: don't care */
    float param1 = 1;
    float param2 = 0;
    float param3 = 0;
    float param4 = 0;
    float param5 = 0;
    float param6 = 0;
    float param7 = 0;

    mavlink_message_t msg;
    mavlink_msg_command_long_pack(
        sys_id, component_id, &msg, target_system, target_component, command,
        confirmation, param1, param2, param3, param4, param5, param6, param7);
    mavlink_send_msg(&msg, fd);

    if (serial_workaround_verbose)
        status("RB5: Sent request_autopilot_capabilities message.");
}

void mavlink_send_ping(int fd)
{
    uint8_t sys_id = RB5_ID;
    uint8_t component_id = MAV_COMP_ID_ONBOARD_COMPUTER;
    uint8_t target_component = MAV_COMP_ID_ALL;

    mavlink_message_t msg;
    mavlink_msg_ping_pack(sys_id, component_id, &msg, 0, 0, 255,
                          target_component);
    mavlink_send_msg(&msg, fd);

    status("RB5: Sent ping message.");
}

void mavlink_send_ack(int fd,
                      uint16_t cmd,
                      uint8_t result,
                      uint8_t progress,
                      int32_t result_param2)
{
    uint8_t sys_id = RB5_ID;
    uint8_t component_id = MAV_COMP_ID_ONBOARD_COMPUTER;
    uint8_t target_system = FCU_ID;
    uint8_t target_component = MAV_COMP_ID_ALL;

    mavlink_message_t msg;
    mavlink_msg_command_ack_pack(sys_id, component_id, &msg, cmd, result,
                                 progress, result_param2, target_system,
                                 target_component);
    mavlink_send_msg(&msg, fd);
}

void mavlink_send_gimbal_manager_info(int fd)
{
    uint8_t sys_id = RB5_ID;
    uint8_t component_id = MAV_COMP_ID_ONBOARD_COMPUTER;

    uint32_t time_boot_ms = 0;
    uint32_t cap_flags = 0;
    uint8_t gimbal_device_id = 1;
    float tilt_max = +M_PI / 2;  // pitch max
    float tilt_min = -M_PI / 2;  // pitch min
    float tilt_rate_max = 0;     // pitch rate max
    float pan_max = +M_PI;       // yaw max
    float pan_min = -M_PI;       // yaw min
    float pan_rate_max = 0;      // yaw rate max

    mavlink_message_t msg;
    mavlink_msg_gimbal_manager_information_pack(
        sys_id, component_id, &msg, time_boot_ms, cap_flags, gimbal_device_id,
        tilt_max, tilt_min, tilt_rate_max, pan_max, pan_min, pan_rate_max);
    mavlink_send_msg(&msg, fd);
}

void mavlink_send_camera_info(int fd)
{
    uint8_t sys_id = RB5_ID;
    uint8_t component_id = MAV_COMP_ID_CAMERA;

    uint32_t time_boot_ms = 0;
    uint8_t *vendor_name = (uint8_t *) get_camera_vendor_name();
    uint8_t *model_name = (uint8_t *) get_camera_model_name();
    uint32_t firmware_version = 0;  // 0 if unknown
    float focal_length = 0;         // mm, NaN if unknown
    float sensor_size_h = 0;        // mm, NaN if unknown
    float sensor_size_v = 0;        // mm, NaN if unknown
    uint16_t resolution_h = 0;      // pix, 0 if unknown
    uint16_t resolution_v = 0;      // pix, 0 if unknown
    uint8_t lens_id = 0;            // 0 if unknown
    uint32_t flags = CAMERA_CAP_FLAGS_CAPTURE_IMAGE;
    uint16_t cam_definition_version = 0;
    char *cam_definition_uri = "";
    uint8_t gimbal_device_id = 1;  // Gimbal's ID associates with the camera

    mavlink_message_t msg;
    mavlink_msg_camera_information_pack(
        sys_id, component_id, &msg, time_boot_ms, vendor_name, model_name,
        firmware_version, focal_length, sensor_size_h, sensor_size_v,
        resolution_h, resolution_v, lens_id, flags, cam_definition_version,
        cam_definition_uri, gimbal_device_id);
    mavlink_send_msg(&msg, fd);
}

#define MSG_SCHEDULER_INIT(freq)          \
    double timer_##freq = get_time_sec(); \
    double period_##freq = 1.0 / (double) freq;

#define MSG_SEND_HZ(freq, expressions)                     \
    double now_##freq = get_time_sec();                    \
    double elapsed_##freq = (now_##freq) - (timer_##freq); \
    if ((elapsed_##freq) >= (period_##freq)) {             \
        (timer_##freq) = (now_##freq);                     \
        expressions                                        \
    }

void *mavlink_tx_thread(void *args)
{
    while (!serial_is_ready())
        sleep(1);

    MSG_SCHEDULER_INIT(1); /* 1Hz */

    while (1) {
        /* clang-format off */
        MSG_SEND_HZ(1,
            mavlink_send_camera_hearbeart(serial);
        );
        /* clang-format on */

        /* Limit CPU usage of the thread with execution frequency of 100Hz */
        usleep(10000); /* 10000us = 10ms */
    }
}
