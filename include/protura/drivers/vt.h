/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_VT_H
#define INCLUDE_DRIVERS_VT_H

#include <protura/types.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/tty.h>

#define VT_PARAM_MAX 10

enum vt_state {
    VT_STATE_BEGIN,
    VT_STATE_ESC,
    VT_STATE_LBRACKET,
    VT_STATE_NUMBERSIGN,
    VT_STATE_OPENPAREN,
    VT_STATE_CLOSEPAREN,
};

enum vt_disp_attrs {
    VT_DISP_BOLD,
    VT_DISP_DIM,
    VT_DISP_UNDERLINE,
    VT_DISP_BLINK,
    VT_DISP_REVERSE,
    VT_DISP_HIDDEN,
};

struct vt {
    struct tty_driver driver;
    struct screen *screen;

    struct tty *tty;

    spinlock_t lock;
    int cur_row, cur_col;

    int scroll_top, scroll_bottom;

    uint8_t fg_color :4;
    uint8_t bg_color :4;
    flags_t cur_attrs;

    int saved_cur_row, saved_cur_col;
    uint8_t saved_fg_color, saved_bg_color;
    flags_t saved_cur_attrs;

    unsigned int wrap_next :1;
    unsigned int dec_private :1;
    unsigned int wrap_on :1;
    unsigned int cursor_is_on :1;
    unsigned int origin_mode :1;
    unsigned int insert_mode :1;

    /* 256 column maximum */
    uint8_t tab_stops[256 / 8];

    int early_init;

    enum vt_state state;

    int esc_param_count;
    int esc_params[VT_PARAM_MAX];
};

void vt_console_kp_register(void);
void vt_console_kp_unregister(void);

void vt_console_early_init(void);

#endif
