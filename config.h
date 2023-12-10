#ifndef __CONFIG_H__
#define __CONFIG_H__

typedef struct {
    char *rtsp_stream_url;
} config_siyi_cam;

void load_siyi_configs(char *yaml_path, config_siyi_cam *config_data);

#endif
