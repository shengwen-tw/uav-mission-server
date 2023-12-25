#ifndef __MAV_RECEIVER_H__
#define __MAV_RECEIVER_H__

#include <stdbool.h>
#include <stdint.h>
#include "mavlink.h"

#define DEF_MAVLINK_CMD(handler_function, id)     \
    {                                             \
        .handler = handler_function, .msg_id = id \
    }

struct mavlink_cmd {
    uint16_t msg_id;
    void (*handler)(mavlink_message_t *msg);
};

void fcu_read_mavlink_msg(uint8_t *buf, size_t nbytes);
bool serial_is_ready(void);

#endif
