#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "siyi_camera.h"

#define SIYI_CAM_IP "192.168.50.25"
#define SIYI_CAM_PORT 37260

#define SIYI_HEADER_SZ 8
#define SIYI_CRC_SZ 2

#define SIYI_ZOOM_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 2)
#define SIYI_GIMBAL_ROTATE_SPEED_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 2)
#define SIYI_GIMBAL_ROTATE_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 4)
#define SIYI_GIMBAL_NEUTRAL_MSG_LEN (SIYI_HEADER_SZ + SIYI_CRC_SZ + 1)

static int siyi_cam_fd;

static const uint16_t crc16_tab[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108,
    0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B,
    0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE,
    0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
    0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D,
    0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5,
    0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
    0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
    0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
    0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13,
    0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E,
    0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1,
    0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
    0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
    0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
    0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882,
    0x28A3, 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E,
    0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
    0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D,
    0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
};

static uint16_t crc16_calculate(uint8_t *ptr, uint32_t len)
{
    uint16_t crc = 0x0;
    uint8_t *byte = ptr;

    for (int i = 0; i < len; i++) {
        crc = ((crc << 8) & 0xff00) ^ crc16_tab[((crc >> 8) & 0xff) ^ *byte];
        byte++;
    }

    return crc;
}

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

void siyi_cam_manual_zoom(uint8_t zoom_integer, uint8_t zoom_decimal)
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
    send(siyi_cam_fd, buf, sizeof(buf), 0);
}

void siyi_cam_gimbal_rotate_speed(int8_t yaw, int8_t pitch)
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
    send(siyi_cam_fd, buf, sizeof(buf), 0);
}

void siyi_cam_gimbal_rotate(int16_t yaw, int16_t pitch)
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
    send(siyi_cam_fd, buf, sizeof(buf), 0);
}

void siyi_cam_gimbal_centering(void)
{
    uint8_t buf[SIYI_GIMBAL_NEUTRAL_MSG_LEN] = {0};
    uint8_t *payload = siyi_cam_pack_common(buf, false, 1, 0, 0x08);

    /* Center position */
    payload[0] = 1; /* Set to 1 by the manual */

    /* CRC */
    uint16_t crc16 = crc16_calculate(buf, SIYI_GIMBAL_NEUTRAL_MSG_LEN - 2);
    memcpy(&payload[1], &crc16, sizeof(crc16));

    /* Send out the message */
    send(siyi_cam_fd, buf, sizeof(buf), 0);
}

void siyi_cam_open(void)
{
    /* Initialize UDP socket */
    if ((siyi_cam_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        printf("%s(): Failed to open the socket\n", __func__);
        exit(1);
    }

    /* Connect to the camera */
    struct sockaddr_in siyi_cam_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(SIYI_CAM_IP),
        .sin_port = htons(SIYI_CAM_PORT),
    };
    if (connect(siyi_cam_fd, (struct sockaddr *) &siyi_cam_addr,
                sizeof(struct sockaddr)) == -1) {
        printf("%s(): Failed to connect to the camera\n", __func__);
        exit(1);
    }

    printf("SIYI camera connected.\n");
}
