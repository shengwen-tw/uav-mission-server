#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

void rtsp_stream_save_image(int camera_id);
void rtsp_stream_change_recording_state(int camera_id);
void rtsp_stream_terminate(void);
void *rtsp_stream_saver(void *args);

#endif
