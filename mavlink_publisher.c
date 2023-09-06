#include "mavlink.h"
#include "serial.h"

void mavlink_send_msg(mavlink_message_t *msg, SerialFd sport)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    size_t len = mavlink_msg_to_send_buffer(buf, msg);

    serial_write(sport, buf, len);
}

void mavlink_send_play_tune(SerialFd sport)
{
    uint8_t sys_id = 1;
    uint8_t component_id = 191;
    uint8_t target_system = 0;
    uint8_t target_component = 0;

    /* check:
     * https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/tunes/tune_definition.desc#L89C44-L89C90
     */
    const char *tune = "MFT240L8 O4aO5dc O4aO5dc O4aO5dc L16dcdcdcdc";
    const char *tune2 = "";

    mavlink_message_t msg;
    mavlink_msg_play_tune_pack(sys_id, component_id, &msg, target_system,
                               target_component, tune, tune2);
    mavlink_send_msg(&msg, sport);
}
