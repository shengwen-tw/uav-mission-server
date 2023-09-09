#ifndef __FCU_H__
#define __FCU_H__

#include <stdbool.h>

void fcu_read_mavlink_msg(uint8_t *buf, size_t nbytes);
bool serial_is_ready(void);

#endif
