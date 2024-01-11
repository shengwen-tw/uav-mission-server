#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

#include "device.h"

struct rtsp_config {
    char *save_path;
    char *codec;
    char *board_name;
    char *rtsp_stream_url;
    char *video_format;
    int image_width;
    int image_height;
};

void rtsp_open(struct camera_dev *cam, void *args);
void rtsp_close(struct camera_dev *cam);
void rtsp_save_image(struct camera_dev *);
void rtsp_change_record_state(struct camera_dev *cam);

#endif
