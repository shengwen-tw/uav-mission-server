#ifndef __SIYI_CAMERA_H__
#define __SIYI_CAMERA_H__

enum {
    SIYI_CAM_ZOOM_IN,
    SIYI_CAM_ZOOM_OUT,
    SIYI_CAM_ZOOM_STOP,
};

void siyi_cam_manual_focus(int8_t zoom, uint16_t zoom_ratio);
void siyi_cam_gimbal_rotate_speed(int8_t yaw, int8_t pitch);
void siyi_cam_gimbal_rotate(int16_t yaw, int16_t pitch);
void siyi_cam_gimbal_rotate_neutral(void);
void siyi_cam_open(void);

#endif
