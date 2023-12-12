#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "mavlink.h"
#include "rtsp_stream.h"
#include "system.h"
#include "uart_server.h"

#define CMD_FIFO "/tmp/cmd_fifo"

// FIXME: Pass directly into pthread
extern int g_argc;
extern char **g_argv;

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

void run_server(int argc, char **argv)
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
    g_argc = argc;
    g_argv = argv;
    pthread_t uart_server_tid;
    pthread_create(&uart_server_tid, NULL, run_uart_server, NULL);

    pthread_t gstreamer_tid;
    pthread_create(&gstreamer_tid, NULL, rtsp_stream_saver, NULL);

    pthread_join(uart_server_tid, NULL);
    pthread_join(gstreamer_tid, NULL);
}

int main(int argc, char const *argv[])
{
    load_configs("configs/siyi_a8_mini.yaml", "siyi");

    if (argc == 2) {
        if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
            help(NULL);
            return 0;
        }
    } else if (argc < 3) {
        help("Too few arguments");
    } else if (argc > 4) {
        help("Too many arguments");
    } else {
        if (strcmp(argv[1], "tune") == 0) {
            /* commander */
            run_commander(argv[2]);
        } else {
            /* server */
            run_server(argc, (char **) argv);
        }
    }

    return 0;
}
