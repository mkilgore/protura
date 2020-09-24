/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/dev.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/vm.h>
#include <protura/mm/kmmap.h>
#include <protura/mm/user_check.h>
#include <protura/fs/file.h>
#include <protura/list.h>

#include <protura/event/device.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/console.h>
#include <protura/fb.h>
#include <protura/video/video.h>
#include <protura/video/fbcon.h>
#include "internal.h"

#define fb_char(fb, r, c) ((fb)->screen.buf + (r) * ((fb)->screen.cols) + (c))

/* The number of MS before we turn the cursor on or off */
#define CURSOR_FLASH 250

/* The number of MS minimum between each screen refrehs */
#define SCREEN_REFRESH_TIMEOUT 50

enum {
    FBCON_CURSOR_IS_ON,
    FBCON_CURSOR_SHOW,

    /* Indicates the screen has already been refreshed in this period and should not be refreshed again */
    FBCON_SCREEN_REFRESHED,

    /* Indicates the screen was modified during the refresh timeout period */
    FBCON_SCREEN_TOUCHED,

    /* This indicates that something blanked the screen and we shouldn't draw to it until it is unblanked */
    FBCON_SCREEN_BLANKED,
};

struct fbcon_screen {
    struct screen screen;
    struct fb_info *info;

    /* Protect against concurrent screen refreshes, and the below members */
    spinlock_t lock;
    struct ktimer cursor_timer;
    struct ktimer refresh_timer;
    flags_t flags;
    int cursor_row, cursor_col;
};

static uint32_t make_color(uint8_t red, uint8_t green, uint8_t blue)
{
    return (red << 16) | (green << 8) | (blue);
}

static uint32_t get_screen_color(uint8_t color)
{
    uint8_t intensity = (color & SCR_BRIGHT)? 0xFF: 0x7F;

    switch (color & ~SCR_BRIGHT) {
    case SCR_BLACK:   return 0;
    case SCR_BLUE:    return make_color(0, 0, intensity);
    case SCR_GREEN:   return make_color(0, intensity, 0);
    case SCR_CYAN:    return make_color(0, intensity, intensity);
    case SCR_RED:     return make_color(intensity, 0, 0);
    case SCR_MAGENTA: return make_color(intensity, 0, intensity);
    case SCR_YELLOW:  return make_color(intensity, intensity, 0);
    case SCR_WHITE:   return make_color(intensity, intensity, intensity);
    }

    return 0;
}

static void __refresh_cursor(struct fbcon_screen *fb)
{
    struct fb_info *info = fb->info;
    uint32_t *fb_data = info->framebuffer;

    if (!flag_test(&fb->flags, FBCON_CURSOR_SHOW) || !flag_test(&fb->flags, FBCON_CURSOR_IS_ON))
        return;

    fb_data += fb->cursor_row * FONT_HEIGHT * info->width + fb->cursor_col * FONT_WIDTH;

    int i, j;
    for (i = FONT_HEIGHT - 4; i < FONT_HEIGHT; i++) {
        uint32_t *line = fb_data + i * info->width;

        for (j = 0; j < FONT_WIDTH; j++)
            line[j] = 0x007F7F7F;
    }
}

static void __refresh_fbcon(struct fbcon_screen *fb)
{
    struct fb_info *info = fb->info;
    uint32_t *fb_data = info->framebuffer;
    int x, y;

    if (flag_test(&fb->flags, FBCON_SCREEN_BLANKED))
        return;

    /* If we're currently in the timeout period, set the TOUCHED flag and don't
     * refresh - the screen will automatically be refreshed when the timeout
     * expires */
    if (flag_test(&fb->flags, FBCON_SCREEN_REFRESHED)) {
        flag_set(&fb->flags, FBCON_SCREEN_TOUCHED);
        return;
    }

    for (x = 0; x < fb->screen.cols; x++) {
        for (y = 0; y < fb->screen.rows; y++) {
            uint32_t *fb_start = fb_data + y * FONT_HEIGHT * info->width + x * FONT_WIDTH;
            int i, j;
            uint32_t fg_color = get_screen_color(screen_fg(fb_char(fb, y, x)->color));
            uint32_t bg_color = get_screen_color(screen_bg(fb_char(fb, y, x)->color));

            for (i = 0; i < FONT_HEIGHT; i++) {
                char c = 0;

                if (fb_char(fb, y, x)->chr < 128)
                    c = protura_fb_font[(int)fb_char(fb, y, x)->chr][i];

                for (j = 0; j < FONT_WIDTH; j++, c <<= 1) {
                    uint32_t *f = fb_start + i * info->width + j;
                    if (c & 0x80)
                        *f = fg_color;
                    else
                        *f = bg_color;
                }
            }

        }
    }

    __refresh_cursor(fb);

    flag_set(&fb->flags, FBCON_SCREEN_REFRESHED);
    timer_add(&fb->refresh_timer, SCREEN_REFRESH_TIMEOUT);
}

static void refresh_timeout_callback(struct ktimer *ktimer)
{
    struct fbcon_screen *fb = container_of(ktimer, struct fbcon_screen, refresh_timer);

    using_spinlock(&fb->lock) {
        flag_clear(&fb->flags, FBCON_SCREEN_REFRESHED);

        /* __refresh_fbcon() will already rearm the timer to run again after
         * refreshing, so it does all the work for us */
        if (flag_test(&fb->flags, FBCON_SCREEN_TOUCHED)) {
            flag_clear(&fb->flags, FBCON_SCREEN_TOUCHED);
            __refresh_fbcon(fb);
        }
    }
}

static void refresh_fbcon(struct screen *fbscreen)
{
    struct fbcon_screen *fb = container_of(fbscreen, struct fbcon_screen, screen);

    using_spinlock(&fb->lock)
        __refresh_fbcon(fb);
}

static void cursor_on(struct screen *screen)
{
    struct fbcon_screen *fb = container_of(screen, struct fbcon_screen, screen);
    using_spinlock(&fb->lock) {
        flag_set(&fb->flags, FBCON_CURSOR_IS_ON);
        timer_add(&fb->cursor_timer, CURSOR_FLASH);

        if (!flag_test(&fb->flags, FBCON_CURSOR_SHOW)) {
            flag_set(&fb->flags, FBCON_CURSOR_SHOW);
            __refresh_fbcon(fb);
        }
    }
}

static void cursor_off(struct screen *screen)
{
    struct fbcon_screen *fb = container_of(screen, struct fbcon_screen, screen);
    using_spinlock(&fb->lock) {
        flag_clear(&fb->flags, FBCON_CURSOR_IS_ON);
        timer_del(&fb->cursor_timer);

        if (flag_test(&fb->flags, FBCON_CURSOR_SHOW)) {
            flag_clear(&fb->flags, FBCON_CURSOR_SHOW);
            __refresh_fbcon(fb);
        }
    }
}

static void move_cursor(struct screen *screen, int row, int col)
{
    struct fbcon_screen *fb = container_of(screen, struct fbcon_screen, screen);
    using_spinlock(&fb->lock) {
        fb->cursor_row = row;
        fb->cursor_col = col;

        if (flag_test(&fb->flags, FBCON_CURSOR_IS_ON) && flag_test(&fb->flags, FBCON_CURSOR_SHOW))
            __refresh_fbcon(fb);
    }
}

static void fbcon_screen_blank(struct fbcon_screen *screen)
{
    using_spinlock(&screen->lock) {
        flag_set(&screen->flags, FBCON_SCREEN_BLANKED);
        timer_del(&screen->refresh_timer);
        timer_del(&screen->cursor_timer);

        memset(screen->info->framebuffer, 0, screen->info->framebuffer_size);
    }
}

static void fbcon_screen_unblank(struct fbcon_screen *screen)
{
    using_spinlock(&screen->lock) {
        if (flag_test(&screen->flags, FBCON_SCREEN_BLANKED)) {
            flag_clear(&screen->flags, FBCON_SCREEN_BLANKED);
            flag_clear(&screen->flags, FBCON_SCREEN_REFRESHED);

            /* The memset is necessary because __refresh_fbcon does not cover
             * the entire screen */
            memset(screen->info->framebuffer, 0, screen->info->framebuffer_size);
            __refresh_fbcon(screen);

            if (flag_test(&screen->flags, FBCON_CURSOR_IS_ON))
                timer_add(&screen->cursor_timer, CURSOR_FLASH);
        }
    }
}

static void cursor_callback(struct ktimer *ktimer)
{
    struct fbcon_screen *fb = container_of(ktimer, struct fbcon_screen, cursor_timer);
    using_spinlock(&fb->lock) {
        flag_toggle(&fb->flags, FBCON_CURSOR_SHOW);

        /* If the cursor is currently being displayed, we refresh the screen and rearm the cursor timer */
        if (flag_test(&fb->flags, FBCON_CURSOR_IS_ON)) {
            __refresh_fbcon(fb);
            timer_add(&fb->cursor_timer, CURSOR_FLASH);
        }
    }
}

static struct fbcon_screen fbcon_screen = {
    .screen = {
        .move_cursor = move_cursor,
        .cursor_on = cursor_on,
        .cursor_off = cursor_off,
        .refresh = refresh_fbcon,
    },
    .lock = SPINLOCK_INIT(),
    .cursor_timer = KTIMER_CALLBACK_INIT(fbcon_screen.cursor_timer, cursor_callback),
    .refresh_timer = KTIMER_CALLBACK_INIT(fbcon_screen.refresh_timer, refresh_timeout_callback),
};

int fbcon_set_framebuffer(struct fb_info *info)
{
    if (fbcon_screen.info != NULL)
        return -EINVAL;

    fbcon_screen.info = info;

    fbcon_screen.screen.rows = info->height / FONT_HEIGHT;
    fbcon_screen.screen.cols = info->width / FONT_WIDTH;
    fbcon_screen.screen.buf = kzalloc(sizeof(*fbcon_screen.screen.buf) * fbcon_screen.screen.rows * fbcon_screen.screen.cols, PAL_KERNEL);

    console_swap_active_screen(&fbcon_screen.screen);

    device_submit_char(KERN_EVENT_DEVICE_ADD, DEV_MAKE(CHAR_DEV_FB, 0));
    return 0;
}

/* Called if the user hits a magic key to forcibly restore the console.
 *
 * It's possible a framebuffer isn't actually active, in which case this is a NOP.
 */
void fbcon_force_unblank(void)
{
    if (!fbcon_screen.info)
        return;

    fbcon_screen_unblank(&fbcon_screen);
}

static int fb_open(struct inode *inode, struct file *filp)
{
    if (!fbcon_screen.info)
        return -ENXIO;

    filp->priv_data = &fbcon_screen;
    return 0;
}

static int fb_ioctl(struct file *filp, int cmd, struct user_buffer arg)
{
    struct task *current = cpu_get_local()->current;
    struct fb_dimensions dimension;
    struct fb_map map;
    struct vm_map *new_mapping;

    struct fbcon_screen *screen = filp->priv_data;
    struct fb_info *disp = screen->info;

    switch (cmd) {
    case FB_IO_GET_DIMENSION:
        dimension.width = disp->width;
        dimension.height = disp->height;
        dimension.bpp = disp->bpp;

        return user_copy_from_kernel(arg, dimension);

    case FB_IO_MAP_FRAMEBUFFER:
        new_mapping = kmalloc(sizeof(*new_mapping), PAL_KERNEL);
        vm_map_init(new_mapping);

        flag_set(&new_mapping->flags, VM_MAP_READ);
        flag_set(&new_mapping->flags, VM_MAP_WRITE);
        flag_set(&new_mapping->flags, VM_MAP_IGNORE);

        address_space_find_region(current->addrspc, disp->framebuffer_size, &new_mapping->addr);
        address_space_vm_map_add(current->addrspc, new_mapping);

        page_table_map_range(current->addrspc->page_dir, new_mapping->addr.start, disp->framebuffer_addr, disp->framebuffer_size / PG_SIZE, new_mapping->flags, PCM_WRITE_COMBINED);

        map.framebuffer = (void *)new_mapping->addr.start;
        map.size = disp->framebuffer_size;

        return user_copy_from_kernel(arg, map);

    case FB_IO_BLANK_SCREEN:
        if (arg.ptr)
            fbcon_screen_blank(screen);
        else
            fbcon_screen_unblank(screen);

        return 0;
    }

    return -ENOTSUP;
}

struct file_ops fb_file_ops = {
    .open = fb_open,
    .ioctl = fb_ioctl,
};
