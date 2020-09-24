/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/snprintf.h>
#include <arch/spinlock.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/vt.h>
#include "vt_internal.h"

#define SCR_DEF_FORGROUND (SCR_WHITE)
#define SCR_DEF_BACKGROUND (SCR_BLACK)

void __vt_clear_color(struct vt *vt, uint8_t color)
{
    int r, c;
    struct screen_char tmp = {
        .chr = 32,
        .color = color,
    };

    for (r = 0; r < vt->screen->rows; r++)
        for (c = 0; c < vt->screen->cols; c++)
            *vt_char(vt, r, c) = tmp;
}

void __vt_clear_to_end(struct vt *vt)
{
    int r, c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    r = vt->cur_row;
    c = vt->cur_col;

    for (; r < vt->screen->rows; r++, c = 0)
        for (; c < vt->screen->cols; c++)
            *vt_char(vt, r, c) = tmp;
}

void __vt_clear_to_cursor(struct vt *vt)
{
    int r, c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (r = 0; r <= vt->cur_row; r++) {
        int end_c = vt->screen->cols;
        if (r == vt->cur_row)
            end_c = vt->cur_col + 1;

        for (c = 0; c < end_c; c++)
            *vt_char(vt, r, c) = tmp;
    }
}

void __vt_clear_line_to_end(struct vt *vt)
{
    int c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (c = vt->cur_col; c < vt->screen->cols; c++)
        *vt_char(vt, vt->cur_row, c) = tmp;
}

void __vt_clear_line_to_cursor(struct vt *vt)
{
    int c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (c = 0; c <= vt->cur_col; c++)
        *vt_char(vt, vt->cur_row, c) = tmp;
}

void __vt_clear_line(struct vt *vt)
{
    int c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (c = 0; c < vt->screen->cols; c++)
        *vt_char(vt, vt->cur_row, c) = tmp;
}

void __vt_clear(struct vt *vt)
{
    __vt_clear_color(vt, screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND));
}

void __vt_updatecur(struct vt *vt)
{
    if (vt->screen->move_cursor)
        vt->screen->move_cursor(vt->screen, vt->cur_row, vt->cur_col);
}

void __vt_scroll(struct vt *vt, int lines)
{
    int min_row = vt->scroll_top;
    int max_row = vt->scroll_bottom;

    if (lines <= 0)
        return;

    int total_scroll_lines = max_row - min_row;

    if (lines > total_scroll_lines)
        lines = total_scroll_lines;

    if (lines < total_scroll_lines)
        memmove(vt_char(vt, min_row, 0),
                vt_char(vt, lines + min_row, 0),
                vt->screen->cols * sizeof(struct screen_char) * (max_row - lines));

    struct screen_char tmp;

    tmp.chr = ' ';
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int r;
    int c;
    for (r = 0; r < lines; r++)
        for (c = 0; c < vt->screen->cols; c++)
            *vt_char(vt, max_row - r - 1, c) = tmp;
}

void __vt_scroll_from_cursor(struct vt *vt, int lines)
{
    int max_row = vt->scroll_bottom;

    int start_row = vt->cur_row;
    int total_rows = max_row - start_row;

    if (lines > total_rows)
        lines = total_rows;

    if (lines < total_rows)
        memmove(vt_char(vt, start_row, 0),
                vt_char(vt, start_row + lines, 0),
                vt->screen->cols * sizeof(struct screen_char) * (total_rows - lines));

    struct screen_char tmp;

    tmp.chr = ' ';
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    int r;
    for (r = max_row - lines; r < max_row; r++)
        for (c = 0; c < vt->screen->cols; c++)
            *vt_char(vt, r, c) = tmp;
}

void __vt_scroll_up_from_cursor(struct vt *vt, int lines)
{
    int max_row = vt->scroll_bottom;

    int start_row = vt->cur_row;
    int total_rows = max_row - start_row;

    if (lines > total_rows)
        lines = total_rows;

    if (lines < total_rows)
        memmove(vt_char(vt, start_row + lines, 0),
                vt_char(vt, start_row, 0),
                vt->screen->cols * sizeof(struct screen_char) * (total_rows - lines));

    struct screen_char tmp;

    tmp.chr = ' ';
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    int r;
    for (r = start_row; r < start_row + lines; r++)
        for (c = 0; c < vt->screen->cols; c++)
            *vt_char(vt, r, c) = tmp;
}

void __vt_shift_left_from_cursor(struct vt *vt, int chars)
{
    int start_col = vt->cur_col;
    int total_cols = vt->screen->cols - start_col;

    if (chars > total_cols)
        chars = total_cols;

    if (chars < total_cols)
        memmove(vt_char(vt, vt->cur_row, start_col),
                vt_char(vt, vt->cur_row, start_col + chars),
                (total_cols - chars) * sizeof(struct screen_char));

    struct screen_char tmp;

    tmp.chr = ' ';
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    for (c = vt->screen->cols - chars; c < vt->screen->cols; c++)
        *vt_char(vt, vt->cur_row, c) = tmp;
}

void __vt_shift_right_from_cursor(struct vt *vt, int chars)
{
    int start_col = vt->cur_col;
    int total_cols = vt->screen->cols - start_col;

    if (chars > total_cols)
        chars = total_cols;

    if (chars < total_cols)
        memmove(vt_char(vt, vt->cur_row, start_col + chars),
                vt_char(vt, vt->cur_row, start_col),
                (vt->screen->cols - chars - start_col) * sizeof(struct screen_char));

    struct screen_char tmp;

    tmp.chr = ' ';
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    for (c = vt->cur_col; c < vt->cur_col + chars; c++)
        *vt_char(vt, vt->cur_row, c) = tmp;
}

#ifdef CONFIG_KERNEL_TESTS
# include "vt_scroll_test.c"
#endif
