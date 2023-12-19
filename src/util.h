#ifndef __UTIL_H__
#define __UTIL_H__

static inline void bound_float(float *val, float max, float min)
{
    if (*val > max)
        *val = max;
    else if (*val < min)
        *val = min;
}

void status(const char *fmt, ...);

#endif
