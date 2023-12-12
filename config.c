#include <stdio.h>
#include <yaml.h>

#include "config.h"

/* clang-format off */
#define READ_PARAM_START()                                     \
    if (0) {
#define READ_PARAM(key, name, type, _retval)                   \
    } else if (strcmp((char *) key, name) == 0) {              \
        void *retval = (void *)_retval;                        \
        switch (type) {                                        \
        case TYPE_STRING:                                      \
            yaml_load_param(&parser, &event, (char **)retval); \
            break;                                             \
        case TYPE_INT: {                                       \
            char *tmp;                                         \
            yaml_load_param(&parser, &event, &tmp);            \
            *(int *)retval = atoi(tmp);                        \
            break;                                             \
        }                                                      \
        }
#define READ_PARAM_END()                                       \
    }
/* clang-format on */

enum {
    TYPE_STRING,
    TYPE_INT,
};

static char *camera_vendor_list[] = {
    "SIYI",
};

static char *camera_model_list[] = {
    "A8-Mini",
};

static int camera_vendor_idx = 0;
static int camera_model_idx = 0;

static struct {
    config_siyi_t config_siyi;
} config_list;

static char *device_name;

static void yaml_load_param(yaml_parser_t *parser,
                            yaml_event_t *event,
                            char **retval)
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

    printf("%s: %s\n", key, value);
    *retval = value;
}

static void load_siyi_configs(char *yaml_path, config_siyi_t *config)
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
            READ_PARAM_START();
            READ_PARAM(key, "board", TYPE_STRING, &config->board);
            READ_PARAM(key, "rtsp_stream_url", TYPE_STRING,
                       &config->rtsp_stream_url);
            READ_PARAM(key, "video_format", TYPE_STRING, &config->video_format);
            READ_PARAM(key, "image_width", TYPE_INT, &config->image_width);
            READ_PARAM(key, "image_height", TYPE_INT, &config->image_height);
            READ_PARAM(key, "siyi_camera_ip", TYPE_STRING,
                       &config->siyi_camera_ip);
            READ_PARAM(key, "siyi_camera_port", TYPE_INT,
                       &config->siyi_camera_port);
            READ_PARAM_END();
        }

        yaml_event_delete(&event);
    } while (event_type != YAML_STREAM_END_EVENT);

    fclose(file);
    yaml_parser_delete(&parser);
}

void load_configs(char *yaml_path, char *device)
{
    device_name = strdup(device);

    if (strcmp("siyi", device) == 0) {
        load_siyi_configs(yaml_path, &config_list.config_siyi);
    } else {
        fprintf(stderr, "Unknown device `%s'\n", device);
        exit(0);
    }
}

char *get_camera_vendor_name(void)
{
    return camera_vendor_list[camera_vendor_idx];
}

char *get_camera_model_name(void)
{
    return camera_model_list[camera_model_idx];
}

void get_config_param(char *name, void *retval)
{
    if (strcmp("siyi", device_name) == 0) {
        config_siyi_t *config = &config_list.config_siyi;

        if (strcmp("rtsp_stream_url", name) == 0) {
            *(char **) retval = config->rtsp_stream_url;
        } else if (strcmp("board", name) == 0) {
            *(char **) retval = config->board;
        } else if (strcmp("video_format", name) == 0) {
            *(char **) retval = config->video_format;
        } else if (strcmp("image_width", name) == 0) {
            *(int *) retval = config->image_width;
        } else if (strcmp("image_height", name) == 0) {
            *(int *) retval = config->image_height;
        } else if (strcmp("siyi_camera_ip", name) == 0) {
            *(char **) retval = config->siyi_camera_ip;
        } else if (strcmp("siyi_camera_port", name) == 0) {
            *(int *) retval = config->siyi_camera_port;
        }
    }
}
