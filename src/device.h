#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdbool.h>
#include <stdint.h>

struct camera_dev;

struct camera_operations {
    /* camera */
    void (*camera_open)(struct camera_dev *cam);
    void (*camera_close)(struct camera_dev *cam);
    void (*camera_save_image)(struct camera_dev *cam);
    void (*camera_change_record_state)(struct camera_dev *cam);
    void (*camera_zoom)(struct camera_dev *cam,
                        uint8_t zoom_integer,
                        uint8_t zoom_decimal);
    /* gimbal */
    void (*gimbal_open)(struct camera_dev *cam);
    void (*gimbal_close)(struct camera_dev *cam);
    void (*gimbal_centering)(struct camera_dev *cam);
    void (*gimbal_rotate)(struct camera_dev *cam, int16_t yaw, int16_t pitch);
};

struct camera_dev {
    int id;
    struct camera_operations *camera_ops;
    void *camera_priv;
    void *gimbal_priv;
};

int register_camera(struct camera_operations *camera_ops);

void camera_open(int id);
void camera_close(int id);
void camera_save_image(int id);
void camera_change_record_state(int id);
void camera_zoom(int id, uint8_t zoom_integer, uint8_t zoom_decimal);

void gimbal_open(int id);
void gimbal_close(int id);
void gimbal_centering(int id);
void gimbal_rotate(int id, int16_t yaw, int16_t pitch);

#endif
