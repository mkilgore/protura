#ifndef INCLUDE_PROTURA_DEV_H
#define INCLUDE_PROTURA_DEV_H

#include <protura/types.h>

#define DEV_MAJOR(dev) ((dev) >> 20)
#define DEV_MINOR(dev) ((dev) & 0xFFFFF)

#define DEV_MAJOR_SET(dev, major) \
    do { \
        (dev) &= 0xFFFFF; \
        (dev) |= (major << 20); \
    } while (0)

#define DEV_MINOR_SET(dev, minor) \
    do { \
        (dev) &= 0xFFF00000; \
        (dev) |= (minor); \
    } while (0)

#define DEV_MAKE(major, minor) (((major) << 20) | (minor))

#define DEV_NONE ((dev_t)0xFFFFFFFF)

#endif
