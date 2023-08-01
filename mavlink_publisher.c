#include "mavlink.h"

#define SYS_ID 1
#define COMP_ID 191

void mavlink_send_play_tune(void)
{
    uint8_t system_id = 1;
    uint8_t component_id = 191;
    const char *tune = "T200 L16 O5 A C O6 E O5 A C O6 E O5 A C O6 E";

    mavlink_message_t msg;
    mavlink_msg_play_tune_v2_pack(system_id, component_id, &msg, 0, 0, 1, tune);

    //uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    //uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
}
