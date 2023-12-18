#ifndef __SIYI_CAMERA_H__
#define __SIYI_CAMERA_H__

#define SIYI_CAM_ZOOM_INT_MIN 0x1
#define SIYI_CAM_ZOOM_INT_MAX 0x1e
#define SIYI_CAM_ZOOM_DEC_MIN 0x0
#define SIYI_CAM_ZOOM_DEC_MAX 0x9

enum {
    SIYI_CAM_FOCUS_IN,
    SIYI_CAM_FOCUS_OUT,
    SIYI_CAM_FOCUS_STOP,
};

void siyi_cam_manual_zoom(uint8_t zoom_integer, uint8_t zoom_decimal);
void siyi_cam_gimbal_rotate_speed(int8_t yaw, int8_t pitch);
void siyi_cam_gimbal_rotate(int16_t yaw, int16_t pitch);
void siyi_cam_gimbal_centering(void);
void siyi_cam_open(void);

#endif
