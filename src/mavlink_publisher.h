#ifndef __MAVLINK_PUBLISHER_H__
#define __MAVLINK_PUBLISHER_H__

#include "serial.h"

void *mavlink_tx_thread(void *args);

void set_video_status(int cam_id);
void reset_video_status(int cam_id);
bool get_video_status(int cam_id);

void mavlink_send_play_tune(int tune_num, int fd);
void mavlink_send_request_autopilot_capabilities(int fd);
void mavlink_send_ping(int fd);
void mavlink_send_ack(uint16_t cmd,
                      uint8_t result,
                      uint8_t progress,
                      int32_t result_param2,
                      uint8_t target_system,
                      uint8_t target_component);
void mavlink_send_gimbal_manager_info(int fd);
void mavlink_request_camera_info(uint8_t target_system,
                                 uint8_t target_component);
void mavlink_send_camera_info(uint8_t target_system, uint8_t target_component);
void mavlink_send_camera_settings(uint8_t target_system,
                                  uint8_t target_component);
void mavlink_send_storage_information(uint8_t target_system,
                                      uint8_t target_component);
void mavlink_send_camera_capture_status(uint8_t target_system,
                                        uint8_t target_component);

#endif
