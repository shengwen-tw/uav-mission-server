#include <stdlib.h>

#include "device.h"
#include "util.h"

#define CAMERA_OPS(id) camera_devs[id].camera_ops

static struct camera_dev camera_devs[CAMERA_NUM_MAX];
static int camera_cnt;

static inline bool device_present(int id)
{
    if (camera_devs[id].camera_priv || camera_devs[id].gimbal_priv)
        return true;
    else
        return false;
}

int register_camera(struct camera_operations *camera_ops)
{
    if (camera_cnt >= CAMERA_NUM_MAX) {
        status("Excedded maximal number of cameras");
        exit(1);
    }

    camera_devs[camera_cnt].id = camera_cnt;
    camera_devs[camera_cnt].camera_ops = camera_ops;
    camera_cnt++;

    return 0;
}

void camera_open(int id, void *args)
{
    if (!CAMERA_OPS(id)->camera_open)
        return;

    CAMERA_OPS(id)->camera_open(&camera_devs[id], args);
}

void camera_close(int id)
{
    if (!device_present(id) || !CAMERA_OPS(id)->camera_close)
        return;

    CAMERA_OPS(id)->camera_close(&camera_devs[id]);
}

void camera_save_image(int id)
{
    if (!device_present(id) || !CAMERA_OPS(id)->camera_save_image)
        return;

    CAMERA_OPS(id)->camera_save_image(&camera_devs[id]);
}

void camera_change_record_state(int id)
{
    if (!device_present(id) || !CAMERA_OPS(id)->camera_change_record_state)
        return;

    CAMERA_OPS(id)->camera_change_record_state(&camera_devs[id]);
}

void camera_zoom(int id, uint8_t zoom_integer, uint8_t zoom_decimal)
{
    if (!device_present(id) || !CAMERA_OPS(id)->camera_zoom)
        return;

    CAMERA_OPS(id)->camera_zoom(&camera_devs[id], zoom_integer, zoom_decimal);
}

void gimbal_open(int id, void *args)
{
    if (!CAMERA_OPS(id)->gimbal_open)
        return;

    CAMERA_OPS(id)->gimbal_open(&camera_devs[id], args);
}

void gimbal_close(int id)
{
    if (!device_present(id) || !CAMERA_OPS(id)->gimbal_close)
        return;

    CAMERA_OPS(id)->gimbal_close(&camera_devs[id]);
}

void gimbal_centering(int id)
{
    if (!device_present(id) || !CAMERA_OPS(id)->gimbal_centering)
        return;

    CAMERA_OPS(id)->gimbal_centering(&camera_devs[id]);
}

void gimbal_rotate(int id, int16_t yaw, int16_t pitch)
{
    if (!device_present(id) || !CAMERA_OPS(id)->gimbal_rotate)
        return;

    CAMERA_OPS(id)->gimbal_rotate(&camera_devs[id], yaw, pitch);
}
