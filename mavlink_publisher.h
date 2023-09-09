#ifndef __MAVLINK_PUBLISHER_H__
#define __MAVLINK_PUBLISHER_H__

#include "serial.h"

void mavlink_send_play_tune(int tune_num, SerialFd sport);
void mavlink_send_request_autopilot_capabilities(SerialFd sport);

#endif
