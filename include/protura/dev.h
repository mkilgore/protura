#ifndef INCLUDE_PROTURA_DEV_H
#define INCLUDE_PROTURA_DEV_H

#include <protura/types.h>

#define DEV_MAJOR(dev) ((dev) >> 8)
#define DEV_MINOR(dev) ((dev) & 0xFF)

#define DEV_MAJOR_SET(dev, major) \
    do { \
        (dev) &= 0xFF; \
        (dev) |= (major << 8); \
    } while (0)

#define DEV_MINOR_SET(dev, minor) \
    do { \
        (dev) &= 0xFF00; \
        (dev) |= (minor); \
    } while (0)

#define DEV_MAKE(major, minor) (((major) << 8) + (minor))

#define DEV_TO_USERSPACE(dev) (((DEV_MAJOR(dev)) << 8) | (DEV_MINOR(dev)))
#define DEV_FROM_USERSPACE(dev) DEV_MAKE(((dev) >> 8), ((dev) & 0xFF))

#define DEV_NONE ((dev_t)0xFFFF)

#endif
