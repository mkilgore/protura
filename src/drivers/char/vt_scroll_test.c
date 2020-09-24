/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for vt_state_machine.c - included directly at the end of vt_state_machine.c
 *
 * These tests are specifically for the scrolling functions.
 */

#include <protura/types.h>
#include <protura/mm/kmalloc.h>
#include <protura/drivers/vt.h>
#include <protura/ktest.h>

struct screen *alloc_fake_screen(void)
{
    struct screen *screen = kzalloc(sizeof(*screen), PAL_KERNEL);
    screen->rows = 25;
    screen->cols = 80;
    screen->buf = kzalloc(screen->rows * screen->cols * sizeof(struct screen_char), PAL_KERNEL);

    return screen;
}

struct vt *alloc_fake_vt(void)
{
    struct vt *vt = kzalloc(sizeof(*vt), PAL_KERNEL);
    spinlock_init(&vt->lock);
    vt->screen = alloc_fake_screen();
    vt->scroll_top = 0;
    vt->scroll_bottom = vt->screen->rows;

    return vt;
}

void free_fake_vt(struct vt *vt)
{
    kfree(vt->screen->buf);
    kfree(vt->screen);
    kfree(vt);
}

void vt_fill_repeat_lines(struct vt *vt)
{
    int r, c;
    for (r = 0; r < vt->screen->rows; r++) {
        for (c = 0; c < vt->screen->cols; c++) {
            vt_char(vt, r, c)->chr = 'A' + r;
            vt_char(vt, r, c)->color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);
        }
    }
}

#define ktest_assert_screen_char(kt, ch, col, scr, r, c) \
    do { \
        char __scr_char_disp[64]; \
        snprintf(__scr_char_disp, sizeof(__scr_char_disp), #scr "[%d][%d].chr", (r), (c)); \
        struct ktest_value v1 = __ktest_make_value(#ch, (ch)); \
        struct ktest_value v2 = __ktest_make_value(__scr_char_disp, vt_char((scr), (r), (c))->chr); \
        ktest_assert_equal_value_func((kt), &v1, &v2, __func__, __LINE__); \
        \
        snprintf(__scr_char_disp, sizeof(__scr_char_disp), #scr "[%d][%d].color", (r), (c)); \
        v1 = __ktest_make_value(#col, (col)); \
        v2 = __ktest_make_value(__scr_char_disp, vt_char((scr), (r), (c))->color); \
        ktest_assert_equal_value_func((kt), &v1, &v2, __func__, __LINE__); \
    } while (0)

static void vt_shift_left_from_cursor_test(struct ktest *kt)
{
    uint8_t def_color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    struct vt *vt = alloc_fake_vt();
    vt->cur_col = KT_ARG(kt, 0, int);
    int chars = KT_ARG(kt, 1, int);

    int c;
    for (c = 0; c < vt->screen->cols; c++) {
        vt_char(vt, 0, c)->chr = 'A' + (c % 26);
        vt_char(vt, 0, c)->color = def_color;
    }

    __vt_shift_left_from_cursor(vt, chars);

    for (c = 0; c < vt->screen->cols; c++) {
        int ch;

        if (c < vt->cur_col)
            ch = 'A' + (c % 26);
        else if (c >= vt->screen->cols - chars)
            ch = ' ';
        else
            ch = 'A' + ((c + chars) % 26);

        ktest_assert_screen_char(kt, ch, def_color, vt, 0, c);
    }

    free_fake_vt(vt);
}

static void vt_shift_right_from_cursor_test(struct ktest *kt)
{
    uint8_t def_color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    struct vt *vt = alloc_fake_vt();
    vt->cur_col = KT_ARG(kt, 0, int);
    int chars = KT_ARG(kt, 1, int);

    int c;
    for (c = 0; c < vt->screen->cols; c++) {
        vt_char(vt, 0, c)->chr = 'A' + (c % 26);
        vt_char(vt, 0, c)->color = def_color;
    }

    __vt_shift_right_from_cursor(vt, chars);

    for (c = 0; c < vt->screen->cols; c++) {
        int ch;

        if (c < vt->cur_col)
            ch = 'A' + (c % 26);
        else if (c < vt->cur_col + chars)
            ch = ' ';
        else
            ch = 'A' + ((c - chars) % 26);

        ktest_assert_screen_char(kt, ch, def_color, vt, 0, c);
    }

    free_fake_vt(vt);
}

static void vt_scroll_up_from_cursor_test(struct ktest *kt)
{
    struct screen_char line_buf[80];
    memset(line_buf, 0, sizeof(line_buf));

    struct vt *vt = alloc_fake_vt();
    vt_fill_repeat_lines(vt);

    vt->cur_row = KT_ARG(kt, 0, int);

    int lines = KT_ARG(kt, 1, int);
    char current;

    __vt_scroll_up_from_cursor(vt, lines);

    int r;
    for (r = 0; r < vt->screen->rows; r++) {
        if (r < vt->cur_row)
            current = 'A' + r;
        else if (r < vt->cur_row + lines)
            current = ' ';
        else
            current = 'A' + r - lines;

        if (current >= 'A' + 25)
            current = ' ';

        int c;
        for (c = 0; c < vt->screen->cols; c++) {
            line_buf[c].chr = current;
            line_buf[c].color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);
        }

        ktest_assert_equal_mem(kt, line_buf, vt_char(vt, r, 0), sizeof(line_buf));
    }

    free_fake_vt(vt);
}

static void vt_scroll_from_cursor_test(struct ktest *kt)
{
    struct screen_char line_buf[80];
    memset(line_buf, 0, sizeof(line_buf));

    struct vt *vt = alloc_fake_vt();
    vt_fill_repeat_lines(vt);

    vt->cur_row = KT_ARG(kt, 0, int);

    int lines = KT_ARG(kt, 1, int);
    char current = 'A';

    __vt_scroll_from_cursor(vt, lines);

    int r;
    for (r = 0; r < vt->screen->rows; r++) {
        if (r == vt->cur_row)
            current = 'A' + vt->cur_row + lines;

        if (current >= 'A' + 25)
            current = ' ';

        int c;
        for (c = 0; c < vt->screen->cols; c++) {
            line_buf[c].chr = current;
            line_buf[c].color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);
        }

        if (current != ' ')
            current++;

        ktest_assert_equal_mem(kt, line_buf, vt_char(vt, r, 0), sizeof(line_buf));
    }

    free_fake_vt(vt);
}

static void vt_scroll_test(struct ktest *kt)
{
    struct screen_char line_buf[80];
    memset(line_buf, 0, sizeof(line_buf));

    int lines = KT_ARG(kt, 0, int);
    char current = 'A' + lines;

    if (lines >= 25)
        current = ' ';

    struct vt *vt = alloc_fake_vt();
    vt_fill_repeat_lines(vt);

    __vt_scroll(vt, lines);

    int r;
    for (r = 0; r < vt->screen->rows; r++) {
        int c;
        for (c = 0; c < vt->screen->cols; c++) {
            line_buf[c].chr = current;
            line_buf[c].color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);
        }

        if (current != ' ') {
            current++;

            if (current == 'A' + 25)
                current = ' ';
        }

        ktest_assert_equal_mem(kt, line_buf, vt_char(vt, r, 0), sizeof(line_buf));
    }

    free_fake_vt(vt);
}

static void vt_scroll_clear_to_cursor_test(struct ktest *kt)
{
    uint8_t def_color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);
    struct vt *vt = alloc_fake_vt();

    vt->cur_row = KT_ARG(kt, 0, int);
    vt->cur_col = KT_ARG(kt, 1, int);

    __vt_clear_to_cursor(vt);

    int r = 0, c = 0;
    for (r = 0; r <= vt->cur_row; r++)
        for (c = 0; c <= vt->cur_col; c++)
            ktest_assert_screen_char(kt, ' ', def_color, vt, r, c);

    for (; r < vt->screen->rows; r++)
        for (; c < vt->screen->cols; c++)
            ktest_assert_screen_char(kt, 0, 0, vt, r, c);

    free_fake_vt(vt);
}

static void vt_scroll_clear_to_end_test(struct ktest *kt)
{
    uint8_t def_color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);
    struct vt *vt = alloc_fake_vt();

    vt->cur_row = KT_ARG(kt, 0, int);
    vt->cur_col = KT_ARG(kt, 1, int);

    __vt_clear_to_end(vt);

    int r = 0, c = 0;
    for (r = 0; r <= vt->cur_row; r++)
        for (c = 0; c < vt->cur_col; c++)
            ktest_assert_screen_char(kt, 0, 0, vt, r, c);

    for (; r < vt->screen->rows; r++)
        for (; c < vt->screen->cols; c++)
            ktest_assert_screen_char(kt, ' ', def_color, vt, r, c);

    free_fake_vt(vt);
}

static void vt_scroll_clear_test(struct ktest *kt)
{
    uint8_t color = screen_make_color(SCR_RED, SCR_BLUE);
    struct vt *vt = alloc_fake_vt();

    __vt_clear_color(vt, color);

    int r, c;
    for (r = 0; r < vt->screen->rows; r++)
        for (c = 0; c < vt->screen->cols; c++)
            ktest_assert_screen_char(kt, ' ', color, vt, r, c);

    free_fake_vt(vt);
}


static const struct ktest_unit vt_scroll_test_units[] = {
    KTEST_UNIT("vt-scroll-clear-color", vt_scroll_clear_test),

    KTEST_UNIT("vt-scroll-clear-to-end", vt_scroll_clear_to_end_test,
            (KT_INT(0),  KT_INT(0)),
            (KT_INT(24), KT_INT(79)),
            (KT_INT(24), KT_INT(0)),
            (KT_INT(0),  KT_INT(79)),
            (KT_INT(1),  KT_INT(1)),
            (KT_INT(5),  KT_INT(10)),
            (KT_INT(24), KT_INT(40)),
            (KT_INT(10), KT_INT(70))),

    KTEST_UNIT("vt-scroll-clear-to-cursor", vt_scroll_clear_to_cursor_test,
            (KT_INT(0),  KT_INT(0)),
            (KT_INT(24), KT_INT(79)),
            (KT_INT(24), KT_INT(0)),
            (KT_INT(0),  KT_INT(79)),
            (KT_INT(1),  KT_INT(1)),
            (KT_INT(5),  KT_INT(10)),
            (KT_INT(24), KT_INT(40)),
            (KT_INT(10), KT_INT(70))),

    KTEST_UNIT("vt-scroll", vt_scroll_test,
            (KT_INT(0)),
            (KT_INT(1)),
            (KT_INT(2)),
            (KT_INT(5)),
            (KT_INT(6)),
            (KT_INT(7)),
            (KT_INT(8)),
            (KT_INT(9)),
            (KT_INT(10)),
            (KT_INT(15)),
            (KT_INT(20)),
            (KT_INT(21)),
            (KT_INT(22)),
            (KT_INT(23)),
            (KT_INT(24)),
            (KT_INT(25)),
            (KT_INT(30)),
            (KT_INT(35))),

    KTEST_UNIT("vt-scroll-from-cursor", vt_scroll_from_cursor_test,
            (KT_INT(0), KT_INT(0)),
            (KT_INT(0), KT_INT(1)),
            (KT_INT(0), KT_INT(10)),
            (KT_INT(0), KT_INT(15)),
            (KT_INT(0), KT_INT(20)),
            (KT_INT(0), KT_INT(25)),
            (KT_INT(0), KT_INT(30)),
            (KT_INT(0), KT_INT(35))),

    KTEST_UNIT("vt-scroll-from-cursor", vt_scroll_from_cursor_test,
            (KT_INT(1), KT_INT(0)),
            (KT_INT(1), KT_INT(1)),
            (KT_INT(1), KT_INT(10)),
            (KT_INT(1), KT_INT(15)),
            (KT_INT(1), KT_INT(24)),
            (KT_INT(1), KT_INT(25))),

    KTEST_UNIT("vt-scroll-from-cursor", vt_scroll_from_cursor_test,
            (KT_INT(24), KT_INT(0)),
            (KT_INT(24), KT_INT(1)),
            (KT_INT(24), KT_INT(10))),

    KTEST_UNIT("vt-scroll-up-from-cursor", vt_scroll_up_from_cursor_test,
            (KT_INT(0), KT_INT(0)),
            (KT_INT(0), KT_INT(1)),
            (KT_INT(0), KT_INT(10)),
            (KT_INT(0), KT_INT(15)),
            (KT_INT(0), KT_INT(20)),
            (KT_INT(0), KT_INT(25)),
            (KT_INT(0), KT_INT(30)),
            (KT_INT(0), KT_INT(35))),

    KTEST_UNIT("vt-scroll-up-from-cursor", vt_scroll_up_from_cursor_test,
            (KT_INT(1), KT_INT(0)),
            (KT_INT(1), KT_INT(1)),
            (KT_INT(1), KT_INT(10)),
            (KT_INT(1), KT_INT(15)),
            (KT_INT(1), KT_INT(24)),
            (KT_INT(1), KT_INT(25))),

    KTEST_UNIT("vt-scroll-up-from-cursor", vt_scroll_up_from_cursor_test,
            (KT_INT(24), KT_INT(0)),
            (KT_INT(24), KT_INT(1)),
            (KT_INT(24), KT_INT(10))),

    KTEST_UNIT("vt-shift-left-from-cursor", vt_shift_left_from_cursor_test,
            (KT_INT(0), KT_INT(0)),
            (KT_INT(0), KT_INT(1)),
            (KT_INT(0), KT_INT(10)),
            (KT_INT(0), KT_INT(50)),
            (KT_INT(0), KT_INT(80)),
            (KT_INT(0), KT_INT(100))),

    KTEST_UNIT("vt-shift-left-from-cursor", vt_shift_left_from_cursor_test,
            (KT_INT(1), KT_INT(0)),
            (KT_INT(1), KT_INT(1)),
            (KT_INT(10), KT_INT(10)),
            (KT_INT(10), KT_INT(50)),
            (KT_INT(10), KT_INT(70)),
            (KT_INT(10), KT_INT(100))),

    KTEST_UNIT("vt-shift-left-from-cursor", vt_shift_left_from_cursor_test,
            (KT_INT(50), KT_INT(70)),
            (KT_INT(79), KT_INT(70)),
            (KT_INT(79), KT_INT(1))),

    KTEST_UNIT("vt-shift-right-from-cursor", vt_shift_right_from_cursor_test,
            (KT_INT(0), KT_INT(0)),
            (KT_INT(0), KT_INT(1)),
            (KT_INT(0), KT_INT(10)),
            (KT_INT(0), KT_INT(50)),
            (KT_INT(0), KT_INT(80)),
            (KT_INT(0), KT_INT(100))),

    KTEST_UNIT("vt-shift-right-from-cursor", vt_shift_right_from_cursor_test,
            (KT_INT(1), KT_INT(0)),
            (KT_INT(1), KT_INT(1)),
            (KT_INT(10), KT_INT(10)),
            (KT_INT(10), KT_INT(50)),
            (KT_INT(10), KT_INT(70)),
            (KT_INT(10), KT_INT(100))),

    KTEST_UNIT("vt-shift-right-from-cursor", vt_shift_right_from_cursor_test,
            (KT_INT(50), KT_INT(70)),
            (KT_INT(79), KT_INT(70)),
            (KT_INT(79), KT_INT(1))),
};

KTEST_MODULE_DEFINE("vt-scroll", vt_scroll_test_units);
