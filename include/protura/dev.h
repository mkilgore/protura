#ifndef INCLUDE_PROTURA_DEV_H
#define INCLUDE_PROTURA_DEV_H

#include <protura/types.h>

#define DEV_MAJOR(dev) ((dev) >> 16)
#define DEV_MINOR(dev) ((dev) & 0xFFFF)

#define DEV_MAJOR_SET(dev, major) \
    do { \
        (dev) &= 0xFFFF; \
        (dev) |= (major << 16); \
    } while (0)

#define DEV_MINOR_SET(dev, minor) \
    do { \
        (dev) &= 0xFFFF0000; \
        (dev) |= (minor); \
    } while (0)

#define DEV_MAKE(major, minor) (((major) << 16) + (minor))

#endif
