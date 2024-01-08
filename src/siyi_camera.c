#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "device.h"
#include "siyi_camera.h"
#include "util.h"

#define SIYI_CAM(cam) ((struct siyi_cam_dev *) (cam)->gimbal_priv)

#define SIYI_HEADER_SZ 8
#define SIYI_CRC_SZ 2

#define SIYI_ZOOM_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 2)
#define SIYI_GIMBAL_ROTATE_SPEED_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 2)
#define SIYI_GIMBAL_ROTATE_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 4)
#define SIYI_GIMBAL_NEUTRAL_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 1)

struct siyi_cam_dev {
    int fd;
};

static uint8_t *siyi_cam_pack_common(uint8_t *buf,
                                     bool ctrl,
                                     uint16_t data_len,
                                     uint16_t seq,
                                     uint8_t cmd_id)
{
    /* STX */
    buf[0] = 0x55;
    buf[1] = 0x66;

    /* CTRL */
    buf[2] = ctrl ? 0 : 1;

    /* DATA LEN */
    buf[3] = ((uint8_t *) &data_len)[0];
    buf[4] = ((uint8_t *) &data_len)[1];


    /* SEQ */
    buf[5] = ((uint8_t *) &seq)[0];
    buf[6] = ((uint8_t *) &seq)[1];

    /* CMD ID */
    buf[7] = cmd_id;

    /* Return the pointer of the payload */
    return &buf[8];
}

static void siyi_cam_manual_zoom(struct camera_dev *cam,
                                 uint8_t zoom_integer,
                                 uint8_t zoom_decimal)
{
    uint8_t buf[SIYI_ZOOM_MSG_LEN] = {0};
    uint8_t *payload = siyi_cam_pack_common(buf, false, 2, 0, 0x0f);

    /* Set integer part of the camera zoom ratio */
    payload[0] = zoom_integer;

    /* Set decimal place of the camera zoom ratio */
    payload[1] = zoom_decimal;

    /* CRC */
    uint16_t crc16 = crc16_calculate(buf, SIYI_ZOOM_MSG_LEN - 2);
    memcpy(&payload[2], &crc16, sizeof(crc16));

    /* Send out the message */
    send(SIYI_CAM(cam)->fd, buf, sizeof(buf), 0);
}

__attribute__((unused)) static void
siyi_cam_gimbal_rotate_speed(struct camera_dev *cam, int8_t yaw, int8_t pitch)
{
    uint8_t buf[SIYI_GIMBAL_ROTATE_SPEED_MSG_LEN] = {0};
    uint8_t *payload = siyi_cam_pack_common(buf, false, 2, 0, 0x07);

    /* Yaw */
    payload[0] = yaw;

    /* Pitch */
    payload[1] = pitch;

    /* CRC */
    uint16_t crc16 = crc16_calculate(buf, SIYI_GIMBAL_ROTATE_SPEED_MSG_LEN - 2);
    memcpy(&payload[2], &crc16, sizeof(crc16));

    /* Send out the message */
    send(SIYI_CAM(cam)->fd, buf, sizeof(buf), 0);
}

static void siyi_cam_gimbal_rotate(struct camera_dev *cam,
                                   int16_t yaw,
                                   int16_t pitch)
{
    uint8_t buf[SIYI_GIMBAL_ROTATE_MSG_LEN] = {0};
    uint8_t *payload = siyi_cam_pack_common(buf, false, 4, 0, 0x0e);

    /* Yaw */
    memcpy(&payload[0], &yaw, sizeof(yaw));

    /* Pitch */
    memcpy(&payload[2], &pitch, sizeof(pitch));

    /* CRC */
    uint16_t crc16 = crc16_calculate(buf, SIYI_GIMBAL_ROTATE_MSG_LEN - 2);
    memcpy(&payload[4], &crc16, sizeof(crc16));

    /* Send out the message */
    send(SIYI_CAM(cam)->fd, buf, sizeof(buf), 0);
}

static void siyi_cam_gimbal_centering(struct camera_dev *cam)
{
    uint8_t buf[SIYI_GIMBAL_NEUTRAL_MSG_LEN] = {0};
    uint8_t *payload = siyi_cam_pack_common(buf, false, 1, 0, 0x08);

    /* Center position */
    payload[0] = 1; /* Set to 1 by the manual */

    /* CRC */
    uint16_t crc16 = crc16_calculate(buf, SIYI_GIMBAL_NEUTRAL_MSG_LEN - 2);
    memcpy(&payload[1], &crc16, sizeof(crc16));

    /* Send out the message */
    send(SIYI_CAM(cam)->fd, buf, sizeof(buf), 0);
}

static void siyi_cam_open(struct camera_dev *cam)
{
    cam->gimbal_priv = malloc(sizeof(struct siyi_cam_dev));

    char *ip = "";
    get_config_param("siyi_camera_ip", &ip);

    int port = 0;
    get_config_param("siyi_camera_port", &port);

    /* Initialize UDP socket */
    if ((SIYI_CAM(cam)->fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        status("%s(): Failed to open the socket", __func__);
        exit(1);
    }

    /* Connect to the camera */
    struct sockaddr_in siyi_cam_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(ip),
        .sin_port = htons(port),
    };
    if (connect(SIYI_CAM(cam)->fd, (struct sockaddr *) &siyi_cam_addr,
                sizeof(struct sockaddr)) == -1) {
        status("%s(): Failed to connect to the camera", __func__);
        exit(1);
    }

    status("SIYI camera connected.");
}

static void siyi_cam_close(struct camera_dev *cam)
{
    close(SIYI_CAM(cam)->fd);
    free(SIYI_CAM(cam));
}

static struct camera_operations siyi_cam_ops = {
    .camera_open = NULL,
    .camera_close = NULL,
    .camera_zoom = siyi_cam_manual_zoom,
    .gimbal_open = siyi_cam_open,
    .gimbal_close = siyi_cam_close,
    .gimbal_centering = siyi_cam_gimbal_centering,
    .gimbal_rotate = siyi_cam_gimbal_rotate,
};

void register_siyi_camera(void)
{
    register_camera(&siyi_cam_ops);
}
