/* UART Server -
 *  A simple program that serves a serial port over TCP to multiple clients.
 */

#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mavlink.h"
#include "mavlink_parser.h"
#include "mavlink_publisher.h"
#include "serial.h"
#include "system.h"

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif /* (defined(__unix__) || defined(unix)) && !defined(USG) */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define closesocket close
#define INVALID_SOCKET (-1)

typedef int Socket;
typedef uint32_t socklen_t;
typedef struct pollfd Waiter;

#define DEFAULT_PORT 8278
#define SERIAL_TIMEOUT 1000

#define ARGS_SERIAL_PORT 1
#define ARGS_SERIAL_CFG 2
#define ARGS_TCP_PORT 3

#define SERIAL_CFG_BAUDRATE_IDX 0
#define SERIAL_CFG_PARITY_IDX 1
#define SERIAL_CFG_DATA_BITS_IDX 2
#define SERIAL_CFG_STOP_BITS_IDX 3
#define SERIAL_CFG_COUNT_MAX 4

#define SERIAL_CFG_DEFAULT_PARITY e_parity_none
#define SERIAL_CFG_DEFAULT_DATA_BITS 8
#define SERIAL_CFG_DEFAULT_STOP_BITS e_stop_bits_one

#define EVENT_CLOSE_INDEX 0
#define EVENT_SERIAL_INDEX 1
#define EVENT_SERVER_INDEX 2
#define EVENT_CLIENT_INDEX 3
#define EVENT_INDEX_COUNT_MAX 4

/**
 * Simple utility macro to get the biggest of two integers.
 */
#define IMAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * Helper type definitions to make the code a bit clearer.
 */
typedef uint32_t Ipv4Addr;
typedef uint16_t InetPort;

/**
 * A structure that holds connected client information.
 */
struct ClientNode {
    int id;        /* Used for quickly identifying a client */
    Ipv4Addr addr; /* Client address for info pretty printing */
    InetPort port; /* Client port for info pretty printing */
    Socket client; /* The client socket */
    struct ClientNode
        *next; /* Pointer to the next client node in the clients list */

    /* MAVLink parser */
    bool parse_me;
};

enum { PLAY_TUNE_CMD };

typedef struct {
    int type;    // Command type
    int arg[4];  // In case the action require some parameters
} mavlink_cmd;

mavlink_status_t mavlink_status;
mavlink_message_t recvd_msg;

/**
 * A utility function that parses a string as an unsigned integer.
 *
 * @param s
 * @param len
 * @param o_result
 *
 * @return
 */
static int get_unsigned(const char *s, size_t len, unsigned *o_result)
{
    unsigned helper;

    if ((!s) || (!len) || (!o_result)) {
        return FALSE;
    }

    helper = *o_result = 0;

    do {
        if (!isdigit(*s)) {
            return FALSE;
        }

        *o_result *= 10;
        *o_result += *s++ - '0';

        /* Make sure we didn't overflow */
        if (*o_result < helper) {
            return FALSE;
        }

        helper = *o_result;
    } while (--len);

    return TRUE;
}

/**
 * A utility function that parses a serial configuration string.
 *
 * @param config_str
 * @param o_config
 *
 * @return
 */
static const char *parse_serial_config(const char *config_str,
                                       struct SerialConfig *o_config)
{
    int cfg_idx = 0;
    const char *start = config_str;

    if ((!config_str) || (!o_config)) {
        return "Internal program error";
    }

    /* Initialise with default values */
    o_config->parity = SERIAL_CFG_DEFAULT_PARITY;
    o_config->data_bits = SERIAL_CFG_DEFAULT_DATA_BITS;
    o_config->stop_bits = SERIAL_CFG_DEFAULT_STOP_BITS;

    /* Parse config elements */
    do {
        const char *next, *end;

        /* Trim start */
        while (isspace(*start)) {
            ++start;
        }

        next = strchr(start, ',');

        if (!next) {
            next = start + strlen(start);
        }

        end = next;

        /* Trim end */
        while ((end > start) && (isspace(end[-1]))) {
            --end;
        }

        /* Parse specific config elements */
        switch (cfg_idx) {
        case SERIAL_CFG_BAUDRATE_IDX:
            if ((!get_unsigned(start, end - start, &o_config->baudrate)) ||
                (!o_config->baudrate)) {
                return "baud rate must be a positive integer";
            }

            break;

        case SERIAL_CFG_PARITY_IDX:
            if (end - start == 1) {
                switch (*start) {
                case 'N':
                    o_config->parity = e_parity_none;
                    break;

                case 'O':
                    o_config->parity = e_parity_odd;
                    break;

                case 'E':
                    o_config->parity = e_parity_even;
                    break;

                case 'M':
                    o_config->parity = e_parity_mark;
                    break;

                case 'S':
                    o_config->parity = e_parity_space;
                    break;

                default:
                    /* Increment end to reach the error condition below */
                    ++end;
                    break;
                }
            }

            if (end - start > 1) {
                return "Parity configuration must be either empty or one of N, "
                       "O, E, M, or S";
            }

            break;

        case SERIAL_CFG_DATA_BITS_IDX:
            if (end - start == 1) {
                switch (*start) {
                case '5':
                case '6':
                case '7':
                case '8':
                    o_config->data_bits = *start - '0';
                    break;

                default:
                    /* Increment end to reach the error condition below */
                    ++end;
                    break;
                }
            }

            if (end - start > 1) {
                return "Data bits must be either empty or one of 5, 6, 7, or 8";
            }

            break;

        case SERIAL_CFG_STOP_BITS_IDX:
            if (end - start > 0) {
                if (memcmp("1.5", start, IMAX(3, end - start)) == 0) {
                    o_config->stop_bits = e_stop_bits_one_half;
                } else if (memcmp("1", start, IMAX(1, end - start)) == 0) {
                    o_config->stop_bits = e_stop_bits_one;
                } else if (memcmp("2", start, IMAX(1, end - start)) == 0) {
                    o_config->stop_bits = e_stop_bits_two;
                } else {
                    return "Stop bits must be either empty or one of 1, 1.5, "
                           "or 2";
                }
            }

            break;

        default:
            return "Invalid serial port configuration string";
        }

        ++cfg_idx;
        start = next + 1;
    } while (start[-1]);

    return NULL;
}

/**
 * A utility function that converts a parity enumeration value to a human
 * readable string.
 */
static const char *parity_to_string(enum SerialParity parity)
{
    switch (parity) {
    case e_parity_none:
        return "None";

    case e_parity_odd:
        return "Odd";

    case e_parity_even:
        return "Even";

    case e_parity_mark:
        return "Mark";

    case e_parity_space:
        return "Space";

    default:
        return "Unknown";
    }
}

/**
 * A utility function that converts a stop bits enumeration value to a human
 * readable string.
 */
static const char *stop_bits_to_string(enum SerialStopBits stop_bits)
{
    switch (stop_bits) {
    case e_stop_bits_one:
        return "1";

    case e_stop_bits_one_half:
        return "1.5";

    case e_stop_bits_two:
        return "2";

    default:
        return "Unknown";
    }
}

/**
 * A utility function that converts an IPv4 address to a human readable string.
 */
static const char *ipaddr_to_string(Ipv4Addr addr)
{
    static char ip[sizeof("255.255.255.255")];

    /* Format the IP address nicely */
    if (snprintf(ip, sizeof(ip), "%u.%u.%u.%u", (addr >> 24) & 0xff,
                 (addr >> 16) & 0xff, (addr >> 8) & 0xff,
                 addr & 0xff) >= (int) sizeof(ip)) {
        return "Unknown";
    }

    return ip;
}

/**
 * A utility function that returns the current timestamp as a string.
 */
static const char *current_timestamp_as_string(void)
{
    static char current_time[sizeof("DD/mm/YYYY HH:MM:SS")];

    time_t ct = time(NULL);
    struct tm *sm = localtime(&ct);

    /* Return an empty timestamp string if something failed */
    if ((!sm) || (strftime(current_time, sizeof(current_time),
                           "%d/%m/%Y %H:%M:%S", sm) >= sizeof(current_time))) {
        return "";
    }

    return current_time;
}

/**
 * A utility function that prints a printf-formatted string to STDOUT with a
 * timestamp.
 */
static void status(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "[%s] ", current_timestamp_as_string());
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");

    va_end(args);
}

/**
 * A utility function that prints a printf-formatted string to STDERR.
 */
static void error(const char *s, ...)
{
    va_list args;
    va_start(args, s);

    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");

    va_end(args);
}

/**
 * A utility function that prints usage information with an optional error
 * message.
 */
static void help(const char *s, ...)
{
    if (s) {
        va_list args;
        va_start(args, s);

        vfprintf(stderr, s, args);
        fprintf(stderr, "\n");

        va_end(args);
    }

    fputs("usage: uart-server [-h] serial_port config_str [tcp_port]\n",
          stderr);

    /* Print only brief usage info if an error message was provided */
    if (!s) {
        fputs("\nUART TCP Server\n\n", stderr);
        fputs("positional arguments:\n", stderr);
        fputs("  serial_port  The serial port to serve\n", stderr);
        fputs(
            "  config_str   The configuration of the serial port: "
            "baudrate[,parity[,data_bits[,stop_bits]]]\n",
            stderr);
        fputs(
            "                 baudrate    The baud rate to be used for the "
            "communication\n",
            stderr);
        fprintf(stderr,
                "                 parity      N, O, E, M, or S (for None, Odd, "
                "Even, Mark, or Space) [%c]\n",
                parity_to_string(SERIAL_CFG_DEFAULT_PARITY)[0]);
        fprintf(stderr, "                 data_bits   5, 6, 7, or 8 [%d]\n",
                SERIAL_CFG_DEFAULT_DATA_BITS);
        fprintf(stderr, "                 stop_bits   1, 1.5, or 2 [%s]\n",
                stop_bits_to_string(SERIAL_CFG_DEFAULT_STOP_BITS));
        fprintf(stderr, "  tcp_port     The port to serve on [%d]\n\n",
                DEFAULT_PORT);
        fputs("optional arguments:\n", stderr);
        fputs("  -h, --help   show this help message and exit\n", stderr);
    }
}

/* Global state variables */
static unsigned char g_cache[1024];
static int g_last_id = -1, g_commanding_client = -1;
static struct ClientNode *g_clients = NULL;
static Waiter g_waiters[EVENT_INDEX_COUNT_MAX];
int cmd_fifo_w, cmd_fifo_r;

/**
 * A utility function that handles client connections.
 */
static int accept_client(Socket server)
{
    struct sockaddr_in remote;
    socklen_t remlen = sizeof(remote);
    struct ClientNode *new_client =
        (struct ClientNode *) malloc(sizeof(struct ClientNode));

    if (!new_client) {
        goto cleanup;
    }

    new_client->parse_me = 0;  // TODO: identify the Gimbal device

    memset(new_client, 0, sizeof(*new_client));

    if (((new_client->client = accept(server, (struct sockaddr *) &remote,
                                      &remlen)) == INVALID_SOCKET) ||
        (remlen != sizeof(remote)) || (remote.sin_family != AF_INET)) {
        goto cleanup;
    }

    memcpy(&new_client->addr, &remote.sin_addr, sizeof(Ipv4Addr));
    new_client->addr = ntohl(new_client->addr);
    new_client->port = ntohs(remote.sin_port);

    new_client->id = ++g_last_id;
    status("Accepted a connection from %s:%u on client ID %d",
           ipaddr_to_string(new_client->addr), new_client->port,
           new_client->id);

    if (!g_clients) {
        g_clients = new_client;
    } else {
        struct ClientNode *current = g_clients;

        while (current->next) {
            current = current->next;
        }

        current->next = new_client;
    }

    return TRUE;

cleanup:
    if (new_client) {
        if (new_client->client != 0) {
            closesocket(new_client->client);
        }

        free(new_client);
        new_client = NULL;
    }

    return FALSE;
}

/**
 * A utility function that handles client termination.
 */
static struct ClientNode *terminate_client(struct ClientNode *node,
                                           struct ClientNode **pointer)
{
    struct ClientNode *next;

    if ((!node) || (!pointer) || (*pointer != node)) {
        return NULL;
    }

    status("Removing client %d...", node->id);

    /* Clear commanding client event wait */
    if (node->id == g_commanding_client) {
        g_waiters[EVENT_CLIENT_INDEX].fd = -1;
    }

    /* Change the pointer of `pointer` before terminating the client
     * to make sure it never points to an invalid client */
    *pointer = next = node->next;

    /* Gracefully shut down the socket */
    shutdown(node->client, 0);
    closesocket(node->client);
    node->client = INVALID_SOCKET;

    free(node);

    return next;
}

/**
 * A utility function that handles serial receive events.
 */
static void send_data_to_clients(SerialFd sport)
{
    struct ClientNode *current = g_clients, **previous = &g_clients;
    long rbytes = serial_read(sport, g_cache, sizeof(g_cache));

    if (rbytes <= 0) {
        return;
    }

    while (current) {
        size_t sent = 0;

        for (;;) {
            long sbytes = send(current->client, (char *) g_cache + sent,
                               (size_t) rbytes - sent, 0);

            if (sbytes < 0) {
                current = terminate_client(current, previous);
                break;
            }

            sent += (size_t) sbytes;

            if (sent == (size_t) rbytes) {
                previous = &(*previous)->next;
                current = current->next;
                break;
            }
        }
    }
}

/**
 * A utility function that handles commanding client data.
 */
static void handle_commanding_client(SerialFd sport)
{
    long rbytes;
    static int last_commander = -1, tryout = 0;

    if (!g_clients) {
        return;
    }

    if (last_commander != g_commanding_client) {
        last_commander = g_commanding_client;
        tryout = 0;
    }

    rbytes = recv(g_clients->client, (char *) g_cache, sizeof(g_cache), 0);

    /* Parse MAVLink message for the gimbal device */
    if (g_clients->parse_me) {
        for (int i = 0; i < rbytes; i++) {
            if (mavlink_parse_char(MAVLINK_COMM_1, g_cache[i], &recvd_msg,
                                   &mavlink_status) == 1) {
                parse_mavlink_msg(&recvd_msg);

                /* Gimbal device only communicates with the server, the
                 * received data has no need to pass to the flight controller */
                return;
            }
        }
    }

    if (rbytes <= 0) {
        if (tryout++ == 3) {
            terminate_client(g_clients, &g_clients);
        }

        return;
    }

    serial_write(sport, g_cache, (size_t) rbytes);
}

/* Pipe ends used to signal that we should gracefully shut down on POSIX systems
 */
static int g_close[2] = {-1, -1};

static void sig_handler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGABRT:
    case SIGTERM: {
        char b = '\0';
        write(g_close[1], &b, 1);
        delete_pidfile();
    } break;
    default:
        break;
    }
}

int run_uart_server(int argc, char const *argv[])
{
    int ret_val = EXIT_FAILURE;

    unsigned port = DEFAULT_PORT;
    struct SerialConfig cfg;

    const char *parse_err = parse_serial_config(argv[ARGS_SERIAL_CFG], &cfg);

    if (parse_err) {
        help(parse_err);
    }
    /* Parse the TCP port if provided */
    else if ((argc == 4) &&
             ((!get_unsigned(argv[ARGS_TCP_PORT], strlen(argv[ARGS_TCP_PORT]),
                             &port) ||
               (port == 0) || (port > 0xffff)))) {
        help("port must be in the range 1-65535");
    } else {
        Socket server;

        if ((server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) ==
            INVALID_SOCKET) {
            error("Failed to open a TCP socket");
        } else {
            struct sockaddr_in bind_addr;

            memset(&bind_addr, 0, sizeof(bind_addr));
            bind_addr.sin_family = AF_INET;
            bind_addr.sin_port = htons(port);

            if (bind(server, (struct sockaddr *) &bind_addr,
                     sizeof(bind_addr)) != 0) {
                error("Failed to bind to port %u", port);
            } else if (listen(server, 10) != 0) {
                error("Failed to start TCP listen");
            } else {
                SerialFd serial =
                    serial_open(argv[ARGS_SERIAL_PORT], &cfg, SERIAL_TIMEOUT);

                if (serial == SERIAL_INVALID_FD) {
                    error("Failed to open the requested serial port");
                } else {
                    size_t event_idx = 0;

                    if (pipe(g_close) < 0) {
                        error("Failed to create shut down event: %s",
                              strerror(errno));
                    } else {
                        g_waiters[EVENT_CLOSE_INDEX].fd = g_close[0];
                        g_waiters[EVENT_SERIAL_INDEX].fd = serial;
                        g_waiters[EVENT_SERVER_INDEX].fd = server;
                        g_waiters[EVENT_CLIENT_INDEX].fd = -1;

                        for (event_idx = 0; event_idx < EVENT_INDEX_COUNT_MAX;
                             ++event_idx) {
                            g_waiters[event_idx].events = POLLIN;
                        }
                    }

                    if (event_idx == EVENT_INDEX_COUNT_MAX) {
                        /* Register for application termination requests to
                         * allow graceful shut down */
                        signal(SIGINT, sig_handler);
                        signal(SIGABRT, sig_handler);
                        signal(SIGTERM, sig_handler);

                        /* Workaround for running in mintty (doesn't really
                         * matter everywhere else because we don't print
                         * that much) */
                        setbuf(stdout, NULL);

                        status(
                            "Serving %s @ %u bps (parity %s, %d data bits, "
                            "and %s stop bits) on port %u",
                            argv[ARGS_SERIAL_PORT], cfg.baudrate,
                            parity_to_string(cfg.parity), cfg.data_bits,
                            stop_bits_to_string(cfg.stop_bits), port);

                        /* Main server loop */
                        for (;;) {
                            int result =
                                poll(g_waiters, EVENT_INDEX_COUNT_MAX, -1);

                            if ((result < 0) && (errno != EINTR)) {
                                error("Failed to wait on event: %d (%s)", errno,
                                      strerror(errno));
                                break;
                            }

                            /* Break if we were requested to shut down */
                            if (g_waiters[EVENT_CLOSE_INDEX].revents &
                                (POLLIN | POLLHUP | POLLERR)) {
                                /* Exit with 0 exit code on graceful shut
                                 * down */
                                ret_val = EXIT_SUCCESS;
                                break;
                            }

                            if (g_waiters[EVENT_CLIENT_INDEX].revents &
                                (POLLHUP | POLLERR)) {
                                terminate_client(g_clients, &g_clients);
                            } else if (g_waiters[EVENT_CLIENT_INDEX].revents &
                                       POLLIN) {
                                handle_commanding_client(serial);
                                g_waiters[EVENT_CLIENT_INDEX].revents = 0;
                            }

                            if (g_waiters[EVENT_SERVER_INDEX].revents &
                                POLLIN) {
                                accept_client(server);
                                g_waiters[EVENT_SERVER_INDEX].revents = 0;
                            }

                            if (g_waiters[EVENT_SERIAL_INDEX].revents &
                                POLLIN) {
                                send_data_to_clients(serial);
                                g_waiters[EVENT_SERIAL_INDEX].revents = 0;
                            }

                            /* Send MAVLink tone command to the flight
                             * control board via serial */
                            mavlink_cmd cmd;
                            int retval = read(cmd_fifo_r, &cmd, sizeof(cmd));
                            if (retval == sizeof(cmd)) {
                                switch (cmd.type) {
                                case PLAY_TUNE_CMD:
                                    mavlink_send_play_tune(cmd.arg[0], serial);
                                    status(
                                        "Send play tone message from server to "
                                        "the "
                                        "flight controller");
                                    break;
                                }
                            }

                            /* Check if we need to listen for a new
                             * commanding client */
                            if ((g_clients) &&
                                (g_commanding_client != g_clients->id)) {
                                g_waiters[EVENT_CLIENT_INDEX].fd =
                                    g_clients->client;
                                g_waiters[EVENT_CLIENT_INDEX].revents = 0;

                                g_commanding_client = g_clients->id;

                                status(
                                    "Client %d @ %s:%u is now in command "
                                    "of the serial port",
                                    g_clients->id,
                                    ipaddr_to_string(g_clients->addr),
                                    g_clients->port);
                            }
                        }

                        /* Gracefully shut down all clients */
                        while (g_clients) {
                            terminate_client(g_clients, &g_clients);
                        }
                    }

                    /* Close both ends of shut down pipe */
                    close(g_close[0]);
                    close(g_close[1]);
                    close(cmd_fifo_w);
                    close(cmd_fifo_r);

                    serial_close(serial);
                }
            }

            closesocket(server);
        }
    }

    return ret_val;
}

void send_signal(int signo)
{
    int pid = read_pidfile();
    kill(pid, signo);
}

#define CMD_FIFO "cmd_fifo"
int main(int argc, char const *argv[])
{
    int ret_val = EXIT_FAILURE;

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
        /* commander */
        if (strcmp(argv[1], "tune") == 0) {
            /* open the command fifo */
            cmd_fifo_w = open(CMD_FIFO, O_RDWR);
            if (cmd_fifo_w == -1) {
                printf("Server is not running.\n\r");
                exit(1);
            }

            /* ask the server to play the tune sequence */
            mavlink_cmd cmd;
            cmd.type = PLAY_TUNE_CMD;
            cmd.arg[0] = atoi(argv[2]);
            write(cmd_fifo_w, &cmd, sizeof(cmd));

            printf("Send tune playing request (%d).\n", cmd.arg[0]);
            return 0;
        }

        /* server */

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
        ret_val = run_uart_server(argc, argv);
    }

    return ret_val;
}
