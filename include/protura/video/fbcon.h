/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_VIDEO_FBCON_H
#define INCLUDE_PROTURA_VIDEO_FBCON_H

struct fb_info {
    void *framebuffer;
    pa_t framebuffer_addr;
    size_t framebuffer_size;

    int width;
    int height;
    int bpp;
};

int fbcon_set_framebuffer(struct fb_info *info);
void fbcon_force_unblank(void);

extern struct file_ops fb_file_ops;

#endif
