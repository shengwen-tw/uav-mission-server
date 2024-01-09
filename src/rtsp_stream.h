#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

#include "device.h"

void rtsp_save_image(struct camera_dev *);
void rtsp_change_record_state(struct camera_dev *cam);
void rtsp_terminate(void);
void *rtsp_saver(void *args);

#endif
