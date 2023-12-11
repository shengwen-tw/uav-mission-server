#ifndef __CONFIG_H__
#define __CONFIG_H__

typedef struct {
    char *rtsp_stream_url;
} config_siyi_cam;

void load_siyi_configs(char *yaml_path, config_siyi_cam *config_data);

char *get_camera_vendor_name(void);
char *get_camera_model_name(void);

#endif
