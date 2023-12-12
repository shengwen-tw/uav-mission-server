#ifndef __CONFIG_H__
#define __CONFIG_H__

typedef struct {
    char *board;

    char *rtsp_stream_url;
    char *video_format;
    int image_width;
    int image_height;

    char *siyi_camera_ip;
    int siyi_camera_port;
} config_siyi_t;

void load_configs(char *yaml_path, char *device);

char *get_camera_vendor_name(void);
char *get_camera_model_name(void);
void get_config_param(char *name, void *retval);

#endif
