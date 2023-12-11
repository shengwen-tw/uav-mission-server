#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

void rtsp_stream_save_image(void);
void rtsp_stream_handle_eos(void);
void *rtsp_stream_saver(void *args);

#endif
