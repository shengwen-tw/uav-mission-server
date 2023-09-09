#include "mavlink.h"
#include "serial.h"
#include "util.h"

#define TUNE_CNT 19

extern bool serial_workaround_verbose;

/* Source:
 * https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/tunes/tune_definition.desc#L89C44-L89C90
 */
/* clang-format off */
char *tune_table[TUNE_CNT] = {
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

void mavlink_send_msg(mavlink_message_t *msg, SerialFd sport)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    size_t len = mavlink_msg_to_send_buffer(buf, msg);

    serial_write(sport, buf, len);
}

void mavlink_send_play_tune(int tune_num, SerialFd sport)
{
    if (tune_num >= TUNE_CNT)
        return;

    uint8_t sys_id = 1;
    uint8_t component_id = 191;
    uint8_t target_system = 0;
    uint8_t target_component = 0;
    const char *tune2 = "";  // extension of the first tune argument

    mavlink_message_t msg;
    mavlink_msg_play_tune_pack(sys_id, component_id, &msg, target_system,
                               target_component, tune_table[tune_num], tune2);
    mavlink_send_msg(&msg, sport);

    status("RB5: Sent play_tune message.");
}

void mavlink_send_request_autopilot_capabilities(SerialFd sport)
{
    uint8_t sys_id = 1;
    uint8_t component_id = 191;
    uint8_t target_system = 0;
    uint8_t target_component = 0;
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
    mavlink_send_msg(&msg, sport);

    if (serial_workaround_verbose)
        status("RB5: Sent request_autopilot_capabilities message.");
}
