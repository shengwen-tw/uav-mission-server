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

int main(int argc, char const *argv[])
{
    /* clang-format off */
    struct option opts[] = {
        {"help", 0, NULL, 'h'},
        {"serial-path", 1, NULL, 's'},
        {"serial-config", 1, NULL, 'b'},
        {"ip-port", 1, NULL, 'p'},
        {"send-tune", 1, NULL, 't'},
        {"print-rc", 0, NULL, 'r'},
    };
    /* clang-format on */

    bool commander_mode = false;
    char *net_port = NULL;
    char *cmd_arg = NULL;

    int c, optidx = 0;
    while ((c = getopt_long(argc, (char **) argv, "p:hrt", opts, &optidx)) !=
           -1) {
        switch (c) {
        case 'h':
            help(NULL);
            return 0;
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

    uart_server_args_t uart_server_args = {
        .net_port = net_port,
    };

    if (commander_mode) {
        run_commander(cmd_arg);
    } else {
        load_serial_configs("configs/serial.yaml");
        load_devices_configs("configs/devices.yaml");
        load_rc_configs("configs/rc.yaml");
        run_server(&uart_server_args);
    }

    return 0;
}
