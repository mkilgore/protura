#ifndef INCLUDE_UAPI_PROTURA_FB_H
#define INCLUDE_UAPI_PROTURA_FB_H

#include <protura/types.h>

struct fb_dimensions {
    __kuint32_t width;
    __kuint32_t height;
    __kuint32_t bpp;
};

struct fb_map {
    void *framebuffer;
    __ksize_t size;
};

#define __FBIO 93

#define FB_IO_GET_DIMENSION ((__FBIO << 8) + 0)
#define FB_IO_MAP_FRAMEBUFFER ((__FBIO << 8) + 1)

/* Argument is a boolean:
 *   true: The screen is cleared and output from the fb console is disabled.
 *   false: fb console is restored
 */
#define FB_IO_BLANK_SCREEN ((__FBIO << 8) + 2)

#endif
