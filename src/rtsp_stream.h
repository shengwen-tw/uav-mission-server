#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

#include "device.h"

void rtsp_stream_save_image(struct camera_dev *);
void rtsp_stream_change_record_state(struct camera_dev *cam);
void rtsp_stream_terminate(void);
void *rtsp_stream_saver(void *args);

#endif
