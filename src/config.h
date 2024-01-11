#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>

typedef struct {
    int min;
    int mid;
    int max;
    bool reverse;
} config_rc_t;

void load_devices_configs(char *yaml_path);
void load_rc_configs(char *yaml_path);

char *get_camera_vendor_name(void);
char *get_camera_model_name(void);

void get_rc_config(int rc_channel, config_rc_t *config);
int get_rc_config_min(int rc_channel);
int get_rc_config_mid(int rc_channel);
int get_rc_config_max(int rc_channel);
bool get_rc_config_reverse(int rc_channel);

#endif
