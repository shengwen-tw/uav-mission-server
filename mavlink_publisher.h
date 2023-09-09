#ifndef __MAVLINK_PUBLISHER_H__
#define __MAVLINK_PUBLISHER_H__

#include "serial.h"

void mavlink_send_play_tune(int tune_num, int fd);
void mavlink_send_request_autopilot_capabilities(int fd);
void mavlink_send_ack(int fd,
                      uint16_t cmd,
                      uint8_t result,
                      uint8_t progress,
                      int32_t result_param2);
#endif
