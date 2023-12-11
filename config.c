#include <stdio.h>

#include <yaml.h>

#include "config.h"

static char *camera_vendor_list[] = {
    "SIYI",
};

static char *camera_model_list[] = {
    "A8-Mini",
};

static int camera_vendor_idx = 0;
static int camera_model_idx = 0;

void load_siyi_configs(char *yaml_path, config_siyi_cam *config_data)
{
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);

    /* Open the yaml file */
    FILE *file = fopen(yaml_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open the YAML file.\n");
        exit(1);
    }

    /* Feed yaml file to the parser */
    yaml_parser_set_input_file(&parser, file);

    yaml_event_t event;

    while (1) {
        /* Get next event */
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Error parsing YAML.\n");
            break;
        }

        /* Check for end of the document */
        if (event.type == YAML_STREAM_END_EVENT) {
            break;
        }

        /* Parse key-value pairs */
        while (1) {
            /* Process key event */
            yaml_parser_parse(&parser, &event);

            if (event.type == YAML_SCALAR_EVENT) {
                /* Bindings */
                if (strcmp((char *) event.data.scalar.value,
                           "rtsp_stream_url") == 0) {
                    config_data->rtsp_stream_url =
                        strdup((char *) event.data.scalar.value);
                    yaml_parser_parse(&parser, &event);
                }
            }

            /* Check for end of the mapping */
            yaml_parser_parse(&parser, &event);
            if (event.type == YAML_MAPPING_END_EVENT) {
                break;
            }
        }
    }

    fclose(file);
    yaml_parser_delete(&parser);
    yaml_event_delete(&event);
}

char *get_camera_vendor_name(void)
{
    return camera_vendor_list[camera_vendor_idx];
}

char *get_camera_model_name(void)
{
    return camera_model_list[camera_model_idx];
}
