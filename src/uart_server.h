#ifndef __UART_SERVER_H__
#define __UART_SERVER_H__

typedef struct {
    char *net_port;
} uart_server_args_t;

typedef struct {
    int type;    // Command type
    int arg[4];  // In case the action require some parameters
} mavlink_cmd;

enum {
    PLAY_TUNE_CMD,
};

void help(const char *s, ...);
void *run_uart_server(void *args);

#endif
