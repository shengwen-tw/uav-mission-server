#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <yaml.h>

#include "config.h"
#include "device.h"
#include "rtsp_stream.h"
#include "serial.h"
#include "siyi_camera.h"

#define READ_PARAM_START(verbose) \
    do {                          \
        bool _verbose = verbose;  \
        if (0) {
#define READ_PARAM(key, name, type, _retval)              \
    }                                                     \
    else if (strcmp((char *) key, name) == 0)             \
    {                                                     \
        char *tmp;                                        \
        void *retval = (void *) _retval;                  \
                                                          \
        yaml_load_param(&parser, &event, &tmp, _verbose); \
                                                          \
        switch (type) {                                   \
        case TYPE_STRING:                                 \
            *(char **) retval = tmp;                      \
            break;                                        \
        case TYPE_INT: {                                  \
            *(int *) retval = atoi(tmp);                  \
            break;                                        \
        case TYPE_BOOL:                                   \
            if (strcmp("true", tmp) == 0) {               \
                *(bool *) retval = true;                  \
            } else if (strcmp("false", tmp) == 0) {       \
                *(bool *) retval = false;                 \
            } else {                                      \
                fprintf(stderr, "Illegal boolean value"); \
                exit(1);                                  \
            }                                             \
            break;                                        \
        }                                                 \
        }
#define READ_PARAM_END()                        \
    }                                           \
    else                                        \
    {                                           \
        fprintf(stderr, "Unknown parameter\n"); \
        exit(1);                                \
    }                                           \
    }                                           \
    while (0)

enum {
    TYPE_STRING,
    TYPE_INT,
    TYPE_BOOL,
};

static char *camera_vendor_list[] = {
    "SIYI",
};

static char *camera_model_list[] = {
    "A8-Mini",
};

static int camera_vendor_idx = 0;
static int camera_model_idx = 0;

struct serial_config {
    char *port;
    int baudrate;
    int parity;
    int data_bits;
    int stop_bits;
};

struct device_config {
    char *yaml;
    char *type;
    bool enabled;
};

static struct serial_config serial_cfg;
static struct device_config devs[CAMERA_NUM_MAX];
static config_rc_t rc_channels[18];

static void yaml_load_param(yaml_parser_t *parser,
                            yaml_event_t *event,
                            char **retval,
                            bool verbose)
{
    char *key = (char *) event->data.scalar.value;

    if (!yaml_parser_parse(parser, event)) {
        fprintf(stderr, "Failed to load configurations\n");
        exit(1);
    }

    if (event->type != YAML_SCALAR_EVENT) {
        fprintf(stderr, "Unexpected YAML input\n");
        exit(1);
    }

    char *value = strdup((char *) event->data.scalar.value);
    *retval = value;

    if (verbose)
        printf("%s: %s\n", key, value);
}

static void load_siyi_configs(char *yaml_path,
                              struct siyi_cam_config *siyi_cam_config,
                              struct rtsp_config *rtsp_config)
{
    /* Open the yaml file */
    FILE *file = fopen(yaml_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open the YAML file.\n");
        exit(1);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_event_type_t event_type;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);

    do {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Failed to load configuration\n");
            exit(1);
        }

        event_type = event.type;
        yaml_char_t *key = event.data.scalar.value;

        if (event_type == YAML_SCALAR_EVENT) {
            READ_PARAM_START(true);
            READ_PARAM(key, "save_path", TYPE_STRING, &rtsp_config->save_path);
            READ_PARAM(key, "board", TYPE_STRING, &rtsp_config->board_name);
            READ_PARAM(key, "rtsp_stream_url", TYPE_STRING,
                       &rtsp_config->rtsp_stream_url);
            READ_PARAM(key, "video_format", TYPE_STRING,
                       &rtsp_config->video_format);
            READ_PARAM(key, "codec", TYPE_STRING, &rtsp_config->codec);
            READ_PARAM(key, "image_width", TYPE_INT, &rtsp_config->image_width);
            READ_PARAM(key, "image_height", TYPE_INT,
                       &rtsp_config->image_height);
            READ_PARAM(key, "siyi_camera_ip", TYPE_STRING,
                       &siyi_cam_config->ip);
            READ_PARAM(key, "siyi_camera_port", TYPE_INT,
                       &siyi_cam_config->port);
            READ_PARAM_END();
        }

        yaml_event_delete(&event);
    } while (event_type != YAML_STREAM_END_EVENT);

    fclose(file);
    yaml_parser_delete(&parser);
}

static void siyi_camera_init(int id)
{
    struct siyi_cam_config siyi_cam_config;
    struct rtsp_config rtsp_config;

    char path[PATH_MAX] = {0};
    sprintf(path, "configs/%s", devs[id].yaml);
    load_siyi_configs(path, &siyi_cam_config, &rtsp_config);

    register_siyi_camera(id);
    camera_open(id, (void *) &rtsp_config);
    gimbal_open(id, (void *) &siyi_cam_config);
    camera_zoom(id, 1, 0);
    gimbal_centering(id);
}

void load_serial_configs(char *yaml_path)
{
    char *stop_bits = "";
    char *parity = "";

    /* Open the yaml file */
    FILE *file = fopen(yaml_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open the YAML file.\n");
        exit(1);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_event_type_t event_type;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);

    do {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Failed to load configuration\n");
            exit(1);
        }

        event_type = event.type;
        yaml_char_t *key = event.data.scalar.value;

        if (event_type == YAML_SCALAR_EVENT) {
            READ_PARAM_START(true);
            READ_PARAM(key, "port", TYPE_STRING, &serial_cfg.port);
            READ_PARAM(key, "baudrate", TYPE_INT, &serial_cfg.baudrate);
            READ_PARAM(key, "parity", TYPE_STRING, &parity);
            READ_PARAM(key, "data-bits", TYPE_INT, &serial_cfg.data_bits);
            READ_PARAM(key, "stop-bits", TYPE_STRING, &stop_bits);
            READ_PARAM_END();
        }

        yaml_event_delete(&event);
    } while (event_type != YAML_STREAM_END_EVENT);

    fclose(file);
    yaml_parser_delete(&parser);

    if (strcmp("N", parity) == 0) {
        serial_cfg.parity = e_parity_none;
    } else if (strcmp("O", parity) == 0) {
        serial_cfg.parity = e_parity_odd;
    } else if (strcmp("E", parity) == 0) {
        serial_cfg.parity = e_parity_even;
    } else if (strcmp("M", parity) == 0) {
        serial_cfg.parity = e_parity_mark;
    } else if (strcmp("S", parity) == 0) {
        serial_cfg.parity = e_parity_space;
    } else {
        fprintf(stderr,
                "Parity configuration must be either empty or one of N, "
                "O, E, M, or S");
        exit(1);
    }

    if (serial_cfg.data_bits < 5 || serial_cfg.data_bits > 8) {
        fprintf(stderr,
                "Data bits must be either empty or one of 5, 6, 7, or 8");
        exit(1);
    }

    if (strcmp(stop_bits, "1.5") == 0) {
        serial_cfg.stop_bits = e_stop_bits_one_half;
    } else if (strcmp(stop_bits, "1") == 0) {
        serial_cfg.stop_bits = e_stop_bits_one;
    } else if (strcmp(stop_bits, "2") == 0) {
        serial_cfg.stop_bits = e_stop_bits_two;
    } else {
        fprintf(stderr,
                "Stop bits must be either empty or one of 1, 1.5, "
                "or 2");
    }
}

#define READ_DEVICE_CONFIG(dev_num)                           \
    READ_PARAM(key, "device" #dev_num "_config", TYPE_STRING, \
               &devs[dev_num].yaml)                           \
    READ_PARAM(key, "device" #dev_num "_type", TYPE_STRING,   \
               &devs[dev_num].type)                           \
    READ_PARAM(key, "device" #dev_num "_enabled", TYPE_BOOL,  \
               &devs[dev_num].enabled)

void load_devices_configs(char *yaml_path)
{
    /* Open the yaml file */
    FILE *file = fopen(yaml_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open the YAML file.\n");
        exit(1);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_event_type_t event_type;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);

    do {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Failed to load configuration\n");
            exit(1);
        }

        event_type = event.type;
        yaml_char_t *key = event.data.scalar.value;

        if (event_type == YAML_SCALAR_EVENT) {
            READ_PARAM_START(false);
            READ_DEVICE_CONFIG(0);
            READ_DEVICE_CONFIG(1);
            READ_DEVICE_CONFIG(2);
            READ_DEVICE_CONFIG(3);
            READ_DEVICE_CONFIG(4);
            READ_DEVICE_CONFIG(5);
            READ_PARAM_END();
        }

        yaml_event_delete(&event);
    } while (event_type != YAML_STREAM_END_EVENT);

    fclose(file);
    yaml_parser_delete(&parser);

    for (int id = 0; id < CAMERA_NUM_MAX; id++) {
        if (devs[id].enabled) {
            if (strcmp("siyi", devs[id].type) == 0) {
                siyi_camera_init(id);
            } else if (strcmp("none", devs[id].type) == 0) {
                continue;
            } else {
                fprintf(stderr, "Unknown device `%s'\n", devs[0].type);
                exit(0);
            }
        }
    }
}

#define READ_RC_CONFIG(channel_num)                          \
    READ_PARAM(key, "ch" #channel_num "_min", TYPE_INT,      \
               &rc_channels[channel_num - 1].min)            \
    READ_PARAM(key, "ch" #channel_num "_mid", TYPE_INT,      \
               &rc_channels[channel_num - 1].mid)            \
    READ_PARAM(key, "ch" #channel_num "_max", TYPE_INT,      \
               &rc_channels[channel_num - 1].max)            \
    READ_PARAM(key, "ch" #channel_num "_reverse", TYPE_BOOL, \
               &rc_channels[channel_num - 1].reverse)

void load_rc_configs(char *yaml_path)
{
    /* Open the yaml file */
    FILE *file = fopen(yaml_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open the YAML file.\n");
        exit(1);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_event_type_t event_type;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);

    do {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Failed to load configuration\n");
            exit(1);
        }

        event_type = event.type;
        yaml_char_t *key = event.data.scalar.value;

        if (event_type == YAML_SCALAR_EVENT) {
            READ_PARAM_START(false);
            READ_RC_CONFIG(1);
            READ_RC_CONFIG(2);
            READ_RC_CONFIG(3);
            READ_RC_CONFIG(4);
            READ_RC_CONFIG(5);
            READ_RC_CONFIG(6);
            READ_RC_CONFIG(7);
            READ_RC_CONFIG(8);
            READ_RC_CONFIG(9);
            READ_RC_CONFIG(10);
            READ_RC_CONFIG(11);
            READ_RC_CONFIG(12);
            READ_RC_CONFIG(13);
            READ_RC_CONFIG(14);
            READ_RC_CONFIG(15);
            READ_RC_CONFIG(16);
            READ_RC_CONFIG(17);
            READ_RC_CONFIG(18);
            READ_PARAM_END();
        }

        yaml_event_delete(&event);
    } while (event_type != YAML_STREAM_END_EVENT);

    fclose(file);
    yaml_parser_delete(&parser);
}

char *get_camera_vendor_name(void)
{
    return camera_vendor_list[camera_vendor_idx];
}

char *get_camera_model_name(void)
{
    return camera_model_list[camera_model_idx];
}

void get_serial_port_config(char **port_name, struct SerialConfig *config)
{
    *port_name = serial_cfg.port;
    config->baudrate = serial_cfg.baudrate;
    config->parity = serial_cfg.parity;
    config->data_bits = serial_cfg.data_bits;
    config->stop_bits = serial_cfg.stop_bits;
}

void get_rc_config(int rc_channel, config_rc_t *config)
{
    if (rc_channel < 1 || rc_channel > 18) {
        printf("Illegal RC channel number %d\n", rc_channel);
        exit(1);
    }

    memcpy(config, &rc_channels[rc_channel - 1], sizeof(*config));
}

int get_rc_config_min(int rc_channel)
{
    config_rc_t config;
    get_rc_config(rc_channel, &config);

    return config.min;
}

int get_rc_config_mid(int rc_channel)
{
    config_rc_t config;
    get_rc_config(rc_channel, &config);

    return config.mid;
}

int get_rc_config_max(int rc_channel)
{
    config_rc_t config;
    get_rc_config(rc_channel, &config);

    return config.max;
}

bool get_rc_config_reverse(int rc_channel)
{
    config_rc_t config;
    get_rc_config(rc_channel, &config);

    return config.reverse;
}
