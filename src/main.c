#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "device.h"
#include "mavlink.h"
#include "mavlink_publisher.h"
#include "rtsp_stream.h"
#include "siyi_camera.h"
#include "system.h"
#include "uart_server.h"

#define CMD_FIFO "/tmp/cmd_fifo"

extern int cmd_fifo_w, cmd_fifo_r;

void run_commander(const char *command)
{
    /* open the command fifo */
    cmd_fifo_w = open(CMD_FIFO, O_RDWR);
    if (cmd_fifo_w == -1) {
        printf("Server is not running.\n\r");
        exit(1);
    }

    /* ask the server to play the tune sequence */
    mavlink_cmd cmd;
    cmd.type = PLAY_TUNE_CMD;
    cmd.arg[0] = atoi(command);
    write(cmd_fifo_w, &cmd, sizeof(cmd));

    printf("Send tune playing request (%d).\n", cmd.arg[0]);
    exit(0);
}

void run_server(uart_server_args_t *uart_server_args)
{
    /* create pid file */
    int mastr_pid = getpid();
    create_pidfile(mastr_pid);

    /* create command fifo */
    mkfifo(CMD_FIFO, 0666);
    cmd_fifo_r = open(CMD_FIFO, O_RDWR | O_NONBLOCK);
    if (cmd_fifo_r == -1) {
        perror("mkfifo");
        exit(1);
    }

    /* start the service */
    pthread_t uart_server_tid;
    pthread_create(&uart_server_tid, NULL, run_uart_server,
                   (void *) uart_server_args);

    pthread_t mavlink_tx_tid;
    pthread_create(&mavlink_tx_tid, NULL, mavlink_tx_thread, NULL);

    pthread_join(uart_server_tid, NULL);
    pthread_join(mavlink_tx_tid, NULL);
}

static void device_init(void)
{
    register_siyi_camera();

    struct siyi_cam_config siyi_cam_config;
    get_config_param("siyi_camera_ip", &siyi_cam_config.ip);
    get_config_param("siyi_camera_port", &siyi_cam_config.port);

    camera_open(0, NULL);
    gimbal_open(0, (void *) &siyi_cam_config);
    camera_zoom(0, 1, 0);
    gimbal_centering(0);
}

int main(int argc, char const *argv[])
{
    /* clang-format off */
    struct option opts[] = {
        {"help", 0, NULL, 'h'},
        {"device", 1, NULL, 'd'},
        {"serial-path", 1, NULL, 's'},
        {"serial-config", 1, NULL, 'b'},
        {"ip-port", 1, NULL, 'p'},
        {"send-tune", 1, NULL, 't'},
        {"print-rc", 0, NULL, 'r'},
    };
    /* clang-format on */

    bool commander_mode = false;
    char *device = NULL;
    char *device_yaml = NULL;
    char *serial_path = NULL;
    char *serial_config = NULL;
    char *net_port = NULL;
    char *cmd_arg = NULL;

    int c, optidx = 0;
    while ((c = getopt_long(argc, (char **) argv, "d:c:s:b:p:hrt", opts,
                            &optidx)) != -1) {
        switch (c) {
        case 'h':
            help(NULL);
            return 0;
        case 'd':
            device = optarg;
            break;
        case 'c':
            device_yaml = optarg;
            break;
        case 's':
            serial_path = optarg;
            break;
        case 'b':
            serial_config = optarg;
            break;
        case 'p':
            net_port = optarg;
            break;
        case 't':
            commander_mode = true;
            cmd_arg = optarg;
            break;
        case 'r':
            break;
        default:
            break;
        }
    }

    if (!commander_mode && !device) {
        printf("Device name must be provided via -d option.\n");
        exit(1);
    }

    if (!commander_mode && !device_yaml) {
        printf("Device config file must be provided via -c option.\n");
        exit(1);
    }

    if (!commander_mode && !serial_path) {
        printf("Serial path must be provided via -s option.\n");
        exit(1);
    }

    if (!commander_mode && !serial_config) {
        printf("Serial config string must be provided via -b option.\n");
        exit(1);
    }

    uart_server_args_t uart_server_args = {
        .serial_path = serial_path,
        .serial_config = serial_config,
        .net_port = net_port,
    };

    if (commander_mode) {
        run_commander(cmd_arg);
    } else {
        load_configs(device_yaml, device);
        load_rc_configs("configs/rc_config.yaml");
        device_init();
        run_server(&uart_server_args);
    }

    return 0;
}
