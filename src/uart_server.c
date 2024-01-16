/* UART Server -
 *  A simple program that serves a serial port over TCP to multiple clients.
 */

#include "uart_server.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif /* (defined(__unix__) || defined(unix)) && !defined(USG) */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "mavlink.h"
#include "mavlink_publisher.h"
#include "mavlink_receiver.h"
#include "rtsp_stream.h"
#include "serial.h"
#include "system.h"

#define closesocket close
#define INVALID_SOCKET (-1)

typedef int Socket;
typedef uint32_t socklen_t;
typedef struct pollfd Waiter;

#define DEFAULT_PORT 8278
#define SERIAL_TIMEOUT 1000

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
#define EVENT_USER_CMD_INDEX 4
#define EVENT_INDEX_COUNT_MAX 5

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
};

serial_t serial;
pthread_mutex_t serial_tx_mtx;

mavlink_status_t mavlink_status;

bool serial_workaround_verbose = true;

/**
 * A utility function that parses a string as an unsigned integer.
 *
 * @param s
 * @param len
 * @param o_result
 *
 * @return
 */
static bool get_unsigned(const char *s, size_t len, unsigned *o_result)
{
    unsigned helper;

    if ((!s) || (!len) || (!o_result))
        return false;

    helper = *o_result = 0;

    do {
        if (!isdigit(*s))
            return false;

        *o_result *= 10;
        *o_result += *s++ - '0';

        /* Make sure we didn't overflow */
        if (*o_result < helper)
            return false;

        helper = *o_result;
    } while (--len);

    return true;
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
void status(const char *fmt, ...)
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
void help(const char *s, ...)
{
    if (s) {
        va_list args;
        va_start(args, s);

        vfprintf(stderr, s, args);
        fprintf(stderr, "\n");

        va_end(args);
    }

    fputs("usage: mission-server [tcp_port]\n", stderr);
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

    if (!new_client)
        goto cleanup;

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

        while (current->next)
            current = current->next;

        current->next = new_client;
    }

    return true;

cleanup:
    if (new_client) {
        if (new_client->client != 0)
            closesocket(new_client->client);

        free(new_client);
        new_client = NULL;
    }

    return false;
}

/**
 * A utility function that handles client termination.
 */
static struct ClientNode *terminate_client(struct ClientNode *node,
                                           struct ClientNode **pointer)
{
    struct ClientNode *next;

    if ((!node) || (!pointer) || (*pointer != node))
        return NULL;

    status("Removing client %d...", node->id);

    /* Clear commanding client event wait */
    if (node->id == g_commanding_client)
        g_waiters[EVENT_CLIENT_INDEX].fd = -1;

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
static void send_data_to_clients(serial_t sport)
{
    struct ClientNode *current = g_clients, **previous = &g_clients;
    long rbytes = serial_read(sport, g_cache, sizeof(g_cache));

    if (rbytes <= 0)
        return;

    read_mavlink_msg(g_cache, rbytes);

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
static void handle_commanding_client(serial_t sport)
{
    long rbytes;
    static int last_commander = -1, tryout = 0;

    if (!g_clients)
        return;

    if (last_commander != g_commanding_client) {
        last_commander = g_commanding_client;
        tryout = 0;
    }

    rbytes = recv(g_clients->client, (char *) g_cache, sizeof(g_cache), 0);

    if (rbytes <= 0) {
        if (tryout++ == 3)
            terminate_client(g_clients, &g_clients);

        return;
    }

    pthread_mutex_lock(&serial_tx_mtx);
    serial_write(sport, g_cache, (size_t) rbytes);
    pthread_mutex_unlock(&serial_tx_mtx);
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
        exit(0);
    } break;
    default:
        break;
    }
}

void read_user_cmd(serial_t sport)
{
    /* Read user command from the FIFO */
    static int received = 0;
    mavlink_cmd user_cmd;
    int rsize = read(cmd_fifo_r, (char *) &user_cmd + received,
                     sizeof(mavlink_cmd) - received);
    received += rsize;

    /* read complete */
    if (received == sizeof(user_cmd)) {
        received = 0;  // Reset

        /* Select an action */
        switch (user_cmd.type) {
        case PLAY_TUNE_CMD:
            mavlink_send_play_tune(user_cmd.arg[0], sport);
            break;
        }
    }
}

void *run_uart_server(void *args)
{
    /* Load UART server arguments */
    uart_server_args_t *uart_server_args = (uart_server_args_t *) args;
    char *serial_path;
    char *net_port = uart_server_args->net_port;

    int ret_val = EXIT_FAILURE;

    unsigned port = DEFAULT_PORT;
    struct SerialConfig cfg;

    get_serial_port_config(&serial_path, &cfg);

    /* Parse the TCP port if provided */
    if (net_port) {
        if ((!get_unsigned(net_port, strlen(net_port), &port) || (port == 0) ||
             (port > 0xffff))) {
            help("port must be in the range 1-65535");
            exit(ret_val);
        }
    }

    Socket server;

    if ((server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) ==
        INVALID_SOCKET) {
        error("Failed to open a TCP socket");
        exit(ret_val);
    }

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);

    if (bind(server, (struct sockaddr *) &bind_addr, sizeof(bind_addr)) != 0) {
        error("Failed to bind to port %u", port);
        goto terminate;
    }
    if (listen(server, 10) != 0) {
        error("Failed to start TCP listen");
        goto terminate;
    }

    pthread_mutex_init(&serial_tx_mtx, NULL);
    serial = serial_open(serial_path, &cfg, SERIAL_TIMEOUT);

    if (serial == SERIAL_INVALID_FD) {
        error("Failed to open the requested serial port");
        goto terminate;
    }

    if (pipe(g_close) < 0) {
        error("Failed to create shut down event: %s", strerror(errno));
        goto terminate;
    }

    g_waiters[EVENT_CLOSE_INDEX].fd = g_close[0];
    g_waiters[EVENT_SERIAL_INDEX].fd = serial;
    g_waiters[EVENT_SERVER_INDEX].fd = server;
    g_waiters[EVENT_USER_CMD_INDEX].fd = cmd_fifo_r;
    g_waiters[EVENT_CLIENT_INDEX].fd = -1;

    for (size_t event_idx = 0; event_idx < EVENT_INDEX_COUNT_MAX; ++event_idx)
        g_waiters[event_idx].events = POLLIN;

    /* Register for application termination requests to allow graceful
     * shut down
     */
    signal(SIGINT, sig_handler);
    signal(SIGABRT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Workaround for running in mintty (doesn't really matter everywhere
     * else because we don't print that much)
     */
    setbuf(stdout, NULL);

    status(
        "Serving %s @ %u bps (parity %s, %d data bits, "
        "and %s stop bits) on port %u",
        serial_path, cfg.baudrate, parity_to_string(cfg.parity), cfg.data_bits,
        stop_bits_to_string(cfg.stop_bits), port);

    /* Wait until the connection is established to the flight controller */
    while (!flight_controller_connected()) {
        /* Send autopilot capabilities message */
        mavlink_send_request_autopilot_capabilities(serial);

        /* Wait for a while */
        usleep(500000); /* 500ms */

        /* Attempt to receive autopilot version message from the flight
         * controller */
        long rbytes = serial_read(serial, g_cache, sizeof(g_cache));
        if (rbytes <= 0)
            continue;
        read_mavlink_msg(g_cache, rbytes);
    }

    /* Main server loop */
    for (;;) {
        int result = poll(g_waiters, EVENT_INDEX_COUNT_MAX, -1);

        if ((result < 0) && (errno != EINTR)) {
            error("Failed to wait on event: %d (%s)", errno, strerror(errno));
            break;
        }

        /* Break if we were requested to shut down */
        if (g_waiters[EVENT_CLOSE_INDEX].revents &
            (POLLIN | POLLHUP | POLLERR)) {
            /* Exit with 0 exit code on graceful shut down */
            ret_val = EXIT_SUCCESS;
            break;
        }

        if (g_waiters[EVENT_CLIENT_INDEX].revents & (POLLHUP | POLLERR)) {
            terminate_client(g_clients, &g_clients);
        } else if (g_waiters[EVENT_CLIENT_INDEX].revents & POLLIN) {
            handle_commanding_client(serial);
            g_waiters[EVENT_CLIENT_INDEX].revents = 0;
        }

        if (g_waiters[EVENT_SERVER_INDEX].revents & POLLIN) {
            accept_client(server);
            g_waiters[EVENT_SERVER_INDEX].revents = 0;
        }

        if (g_waiters[EVENT_SERIAL_INDEX].revents & POLLIN) {
            send_data_to_clients(serial);
            g_waiters[EVENT_SERIAL_INDEX].revents = 0;
        }

        /* Event of receiving user commands */
        if (g_waiters[EVENT_USER_CMD_INDEX].revents & POLLIN) {
            read_user_cmd(serial);
            g_waiters[EVENT_USER_CMD_INDEX].revents = 0;
        }

        /* Check if we need to listen for a new commanding client */
        if ((g_clients) && (g_commanding_client != g_clients->id)) {
            g_waiters[EVENT_CLIENT_INDEX].fd = g_clients->client;
            g_waiters[EVENT_CLIENT_INDEX].revents = 0;

            g_commanding_client = g_clients->id;

            status("Client %d @ %s:%u is now in command of the serial port",
                   g_clients->id, ipaddr_to_string(g_clients->addr),
                   g_clients->port);
        }
    }

    /* Gracefully shut down all clients */
    while (g_clients)
        terminate_client(g_clients, &g_clients);

    /* Close both ends of shut down pipe */
    close(g_close[0]);
    close(g_close[1]);
    close(cmd_fifo_w);
    close(cmd_fifo_r);

    serial_close(serial);

terminate:
    closesocket(server);
    exit(ret_val);
}

void send_signal(int signo)
{
    int pid = read_pidfile();
    kill(pid, signo);
}
