#ifndef __COMMON_H__
#define __COMMON_H__

static inline void bound_float(float *val, float max, float min)
{
    if (*val > max)
        *val = max;
    else if (*val < min)
        *val = min;
}

#endif
