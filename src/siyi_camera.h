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

struct siyi_cam_config {
    char *ip;
    int port;
};

void register_siyi_camera(void);

#endif
