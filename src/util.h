#ifndef __UTIL_H__
#define __UTIL_H__

#include <sys/time.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static inline void bound_float(float *val, float max, float min)
{
    if (*val > max)
        *val = max;
    else if (*val < min)
        *val = min;
}

static inline double get_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (double) tv.tv_sec + (double) tv.tv_usec * 1e-6;
}

void status(const char *fmt, ...);

#endif
