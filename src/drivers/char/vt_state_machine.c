/*
 * Copyright (C) 2015 Matt Kilgore
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

static uint8_t vt_create_cur_color(struct vt *vt)
{
    uint8_t fg = vt->fg_color, bg = vt->bg_color;

    if (flag_test(&vt->cur_attrs, VT_DISP_BOLD))
        fg |= SCR_BRIGHT;

    if (flag_test(&vt->cur_attrs, VT_DISP_REVERSE)) {
        uint8_t tmp = fg;
        fg = bg;
        bg = tmp;
    }

    return screen_make_color(fg, bg);
}

static void __vt_putchar_nocursor(struct vt *vt, char ch)
{
    uint8_t r = vt->cur_row;
    uint8_t c = vt->cur_col;
    int next_tab;

    switch (ch) {
    case '\r':
        c = 0;
        vt->wrap_next = 0;
        break;

    case '\n':
    case 11:
    case 12:
        if (r == vt->scroll_bottom - 1)
            __vt_scroll(vt, 1);
        else
            r++;

        vt->wrap_next = 0;
        break;

    case '\t':
        next_tab = bit_find_next_set(vt->tab_stops, sizeof(vt->tab_stops), c + 1);
        if (next_tab != -1)
            c = next_tab;
        else
            c = SCR_COLS - 1;

        vt->wrap_next = 0;
        break;

    case '\b':
        if (c > 0)
            c--;

        vt->wrap_next = 0;
        break;

    default:
        if (vt->wrap_next && c == SCR_COLS - 1 && vt->wrap_on) {
            vt->wrap_next = 0;
            c = 0;
            r++;

            if (r == vt->scroll_bottom) {
                __vt_scroll(vt, 1);
                r--;
            }

            /* if the row is outside the scrolling region, we have to make sure
             * we don't go past SCR_ROWS, but we don't scroll in this case */
            if (r == SCR_ROWS)
                r--;
        }

        if (vt->insert_mode)
            __vt_shift_right_from_cursor(vt, 1);

        vt->screen->buf[r][c].color = vt_create_cur_color(vt);
        vt->screen->buf[r][c].chr = ch;

        if (c == SCR_COLS - 1)
            vt->wrap_next = 1;
        else
            c++;

        break;
    }

    vt->cur_col = c;
    vt->cur_row = r;
}

/*
 * Sets the cursor position to the provided value. The value is checked for
 * correctness.
 */
static void __vt_set_cursor(struct vt *vt, int new_row, int new_col)
{
    int max_row = vt->origin_mode? vt->scroll_bottom: SCR_ROWS;
    int min_row = vt->origin_mode? vt->scroll_top: 0;

    if (new_row < min_row)
        vt->cur_row = min_row;
    else if (new_row > max_row - 1)
        vt->cur_row = max_row - 1;
    else
        vt->cur_row = new_row;

    if (new_col < 0)
        vt->cur_col = 0;
    else if (new_col > SCR_COLS - 1)
        vt->cur_col = SCR_COLS - 1;
    else
        vt->cur_col = new_col;

    vt->wrap_next = 0;
}

/* The arguments to this function are relative to the scrolling regin when
 * origin_mode is on */
static void __vt_set_cursor_origin_relative(struct vt *vt, int new_row, int new_col)
{
    if (vt->origin_mode)
        __vt_set_cursor(vt, new_row + vt->scroll_top, new_col);
    else
        __vt_set_cursor(vt, new_row, new_col);
}

static int attribute_to_screen_color(int color)
{
    switch (color) {
    case 0: return SCR_BLACK;
    case 1: return SCR_RED;
    case 2: return SCR_GREEN;
    case 3: return SCR_YELLOW;
    case 4: return SCR_BLUE;
    case 5: return SCR_MAGENTA;
    case 6: return SCR_CYAN;
    case 7: return SCR_WHITE;
    }
    return 0;
}

/* Handles ESC[m commands */
static void vt_esc_set_attributes(struct vt *vt, char cmd)
{
    int i;

    for (i = 0; i < vt->esc_param_count; i++) {
        switch (vt->esc_params[i]) {
        case 0:
            vt->cur_attrs = 0;
            vt->fg_color = SCR_DEF_FORGROUND;
            vt->bg_color = SCR_DEF_BACKGROUND;
            break;

        case 1:
            flag_set(&vt->cur_attrs, VT_DISP_BOLD);
            break;
        case 22:
            flag_clear(&vt->cur_attrs, VT_DISP_BOLD);
            break;

        case 5:
            flag_set(&vt->cur_attrs, VT_DISP_BLINK);
            break;
        case 25:
            flag_clear(&vt->cur_attrs, VT_DISP_BLINK);
            break;

        case 4:
            flag_set(&vt->cur_attrs, VT_DISP_UNDERLINE);
            break;
        case 24:
            flag_clear(&vt->cur_attrs, VT_DISP_UNDERLINE);
            break;

        case 7:
            flag_set(&vt->cur_attrs, VT_DISP_REVERSE);
            break;
        case 27:
            flag_clear(&vt->cur_attrs, VT_DISP_REVERSE);
            break;

        case 30 ... 37:
            vt->fg_color = attribute_to_screen_color(vt->esc_params[i] - 30);
            break;
        case 39:
            vt->fg_color = SCR_DEF_FORGROUND;
            break;

        case 40 ... 47:
            vt->bg_color = attribute_to_screen_color(vt->esc_params[i] - 40);
            break;
        case 49:
            vt->bg_color = SCR_DEF_BACKGROUND;
            break;
        }
    }
}

/* Handles ESC[H and ESC[f commands */
static void vt_esc_cursor_move(struct vt *vt, char cmd)
{
    int new_row = vt->esc_params[0];
    int new_col = vt->esc_params[1];
    if (new_row)
        new_row--;

    if (new_col)
        new_col--;

    __vt_set_cursor_origin_relative(vt, new_row, new_col);
}

/* Handles ESC[A, ESC[B, ESC[C, ESC[D */
static void vt_esc_cursor_cmd(struct vt *vt, char cmd)
{
    int count = 1;
    if (vt->esc_params[0])
        count = vt->esc_params[0];

    switch (cmd) {
    case 'A': return __vt_set_cursor(vt, vt->cur_row - count, vt->cur_col);

    case 'e':
    case 'B': return __vt_set_cursor(vt, vt->cur_row + count, vt->cur_col);

    case 'a':
    case 'C': return __vt_set_cursor(vt, vt->cur_row, vt->cur_col + count);

    case 'D': return __vt_set_cursor(vt, vt->cur_row, vt->cur_col - count);

    case 'E': return __vt_set_cursor(vt, vt->cur_row + count, 0);

    case 'F': return __vt_set_cursor(vt, vt->cur_row - count, 0);

    case 'G':
    case '`': return __vt_set_cursor(vt, vt->cur_row, count - 1);

    case 'd': return __vt_set_cursor(vt, count - 1, vt->cur_col);
    }
}

static void vt_esc_disp(struct vt *vt, char cmd)
{
    switch (vt->esc_params[0]) {
    case 0: return __vt_clear_to_end(vt);
    case 1: return __vt_clear_to_cursor(vt);
    case 2: return __vt_clear(vt);
    }
}

static void vt_esc_clear_line(struct vt *vt, char cmd)
{
    switch (vt->esc_params[0]) {
    case 0: return __vt_clear_line_to_end(vt);
    case 1: return __vt_clear_line_to_cursor(vt);
    case 2: return __vt_clear_line(vt);
    }
}

static void vt_tty_write_string(struct vt *vt, const char *str)
{
    /* When we're in the booting "early init" stage, the tty may not yet have
     * been created */
    if (vt->tty)
        tty_add_input(vt->tty, str, strlen(str));
}

static void vt_esc_report(struct vt *vt, char cmd)
{
    char buf[32];

    switch (vt->esc_params[0]) {
    case 6:
        if (vt->origin_mode)
            snprintf(buf, sizeof(buf), "\033[%d;%dR", vt->cur_row + 1 - vt->scroll_bottom, vt->cur_col + 1);
        else
            snprintf(buf, sizeof(buf), "\033[%d;%dR", vt->cur_row + 1, vt->cur_col + 1);

        return vt_tty_write_string(vt, buf);

    case 7:
        return vt_tty_write_string(vt, "\033[0n");
    }
}

static void vt_esc_who_are_you(struct vt *vt, char cmd)
{
    vt_tty_write_string(vt, "\033[?6c");
}

static void vt_esc_set_mode(struct vt *vt, char cmd)
{
    int mode_on = 1;
    if (cmd == 'l')
        mode_on = 0;

    switch (vt->esc_params[0]) {
    case 4:
        vt->insert_mode = mode_on;
        break;

    case 25:
        vt->cursor_is_on = mode_on;

        if (mode_on) {
            if (vt->screen->cursor_on)
                (vt->screen->cursor_on) ();
        } else {
            if (vt->screen->cursor_off)
                (vt->screen->cursor_off) ();
        }
        break;
    }
}

static void vt_esc_delete_line(struct vt *vt, char cmd)
{
    int lines = 1;
    if (vt->esc_params[0])
        lines = vt->esc_params[0];

    __vt_scroll_from_cursor(vt, lines);
}

static void vt_esc_delete_character(struct vt *vt, char cmd)
{
    int chars = 1;
    if (vt->esc_params[0])
        chars = vt->esc_params[0];

    __vt_shift_left_from_cursor(vt, chars);
}

static void vt_esc_insert_line(struct vt *vt, char cmd)
{
    int lines = 1;
    if (vt->esc_params[0])
        lines = vt->esc_params[0];

    __vt_scroll_up_from_cursor(vt, lines);
}

static void vt_esc_insert_character(struct vt *vt, char cmd)
{
    int chars = 1;
    if (vt->esc_params[0])
        chars = vt->esc_params[0];

    __vt_shift_right_from_cursor(vt, chars);
}

static void vt_esc_reverse_linefeed(struct vt *vt, char cmd)
{
    if (vt->cur_row > vt->scroll_top)
        __vt_set_cursor(vt, vt->cur_row - 1, vt->cur_col);
    else
        __vt_scroll_up_from_cursor(vt, 1);
}

static void vt_save_cursor(struct vt *vt, char cmd)
{
    vt->saved_cur_row = vt->cur_row;
    vt->saved_cur_col = vt->cur_col;
    vt->saved_fg_color = vt->fg_color;
    vt->saved_bg_color = vt->bg_color;
    vt->saved_cur_attrs = vt->cur_attrs;
}

static void vt_restore_cursor(struct vt *vt, char cmd)
{
    vt->saved_fg_color = vt->fg_color;
    vt->saved_bg_color = vt->bg_color;
    vt->cur_attrs = vt->saved_cur_attrs;

    __vt_set_cursor(vt, vt->saved_cur_row, vt->saved_cur_col);
    vt->wrap_next = 0;
}

static void vt_set_scrolling_region(struct vt *vt, char cmd)
{
    int new_top = vt->esc_params[0];
    int new_bottom = vt->esc_params[1];
    if (new_top)
        new_top--;

    if (new_bottom)
        new_bottom--;
    else
        new_bottom = SCR_ROWS - 1;

    if (new_bottom >= SCR_ROWS)
        return;

    /* The selected region must be at least 2 lines */
    if (new_top >= new_bottom)
        return;

    /* The actual stored region is till one past the top.
     *
     * This makes logic a little simpler. Normally we deal with SCR_ROWS, which
     * is also one past the top */
    vt->scroll_top = new_top;
    vt->scroll_bottom = new_bottom + 1;

    __vt_set_cursor_origin_relative(vt, 0, 0);
}

static void vt_set_dec_setting(struct vt *vt, char cmd)
{
    switch (vt->esc_params[0]) {
    case 7:
        vt->wrap_on = 1;
        break;

    case 6:
        vt->origin_mode = 1;
        __vt_set_cursor_origin_relative(vt, 0, 0);
        break;

    case 3: /* switch to 132 columns - unsupported */
        __vt_clear(vt);
        __vt_set_cursor_origin_relative(vt, 0, 0);
        break;
    }
}

static void vt_unset_dec_setting(struct vt *vt, char cmd)
{
    switch (vt->esc_params[0]) {
    case 7:
        vt->wrap_on = 0;
        break;

    case 6:
        vt->origin_mode = 0;
        __vt_set_cursor_origin_relative(vt, 0, 0);
        break;

    case 3: /* switch to 80 columns */
        __vt_clear(vt);
        __vt_set_cursor_origin_relative(vt, 0, 0);
        break;
    }
}

static void vt_unset_tab(struct vt *vt, char cmd)
{
    switch (vt->esc_params[0]) {
    case 0:
        bit_clear(vt->tab_stops, vt->cur_col);
        break;

    case 3:
        memset(vt->tab_stops, 0, sizeof(vt->tab_stops));
        break;
    }
}

static void vt_reset(struct vt *vt)
{
    vt->fg_color = SCR_DEF_FORGROUND;
    vt->bg_color = SCR_DEF_BACKGROUND;
    vt->cur_attrs = 0;
    vt->cur_row = 0;
    vt->cur_col = 0;

    vt->wrap_on = 1;
    vt->wrap_next = 0;

    vt->origin_mode = 0;
    vt->scroll_top = 0;
    vt->scroll_bottom = SCR_ROWS;

    vt->saved_cur_col = 0;
    vt->saved_cur_row = 0;
    vt->saved_cur_attrs = 0;

    vt->dec_private = 0;
    vt->insert_mode = 0;
    vt->cursor_is_on = 1;

    memset(vt->tab_stops, 0, sizeof(vt->tab_stops));
    bit_set(vt->tab_stops, 0);
    bit_set(vt->tab_stops, 8);
    bit_set(vt->tab_stops, 16);
    bit_set(vt->tab_stops, 24);
    bit_set(vt->tab_stops, 32);
    bit_set(vt->tab_stops, 40);
    bit_set(vt->tab_stops, 48);
    bit_set(vt->tab_stops, 56);
    bit_set(vt->tab_stops, 64);
    bit_set(vt->tab_stops, 72);
    bit_set(vt->tab_stops, 80);

    __vt_clear(vt);
    __vt_updatecur(vt);
}

static void (*lbracket_table[256]) (struct vt *, char) = {
    ['m'] = vt_esc_set_attributes,
    ['H'] = vt_esc_cursor_move,
    ['f'] = vt_esc_cursor_move,
    ['A'] = vt_esc_cursor_cmd,
    ['e'] = vt_esc_cursor_cmd,
    ['B'] = vt_esc_cursor_cmd,
    ['a'] = vt_esc_cursor_cmd,
    ['C'] = vt_esc_cursor_cmd,
    ['D'] = vt_esc_cursor_cmd,
    ['E'] = vt_esc_cursor_cmd,
    ['F'] = vt_esc_cursor_cmd,
    ['G'] = vt_esc_cursor_cmd,
    ['`'] = vt_esc_cursor_cmd,
    ['d'] = vt_esc_cursor_cmd,
    ['J'] = vt_esc_disp,
    ['n'] = vt_esc_report,
    ['K'] = vt_esc_clear_line,
    ['l'] = vt_esc_set_mode,
    ['h'] = vt_esc_set_mode,
    ['M'] = vt_esc_delete_line,
    ['P'] = vt_esc_delete_character,
    ['L'] = vt_esc_insert_line,
    ['@'] = vt_esc_insert_character,
    ['c'] = vt_esc_who_are_you,
    ['r'] = vt_set_scrolling_region,
    ['g'] = vt_unset_tab,
};

static void (*lbracket_table_dec_private[256]) (struct vt *, char) = {
    ['h'] = vt_set_dec_setting,
    ['l'] = vt_unset_dec_setting,
};

static void vt_state_lbracket(struct vt *vt, char ch)
{
    switch (ch) {
    case '0' ... '9':
        vt->esc_params[vt->esc_param_count] = vt->esc_params[vt->esc_param_count] * 10 + (ch - '0');
        break;

    case ';':
        vt->esc_param_count++;
        break;

    case '?':
        vt->dec_private = 1;
        break;

    default:
        vt->esc_param_count++;

        if (vt->dec_private) {
            if (lbracket_table_dec_private[(int)ch])
                (lbracket_table_dec_private[(int)ch]) (vt, ch);
            else
                kp(KP_NORMAL, "Unhandled esc[? '%c', %d\n", ch, ch);
        } else {
            if (lbracket_table[(int)ch])
                (lbracket_table[(int)ch]) (vt, ch);
            else
                kp(KP_NORMAL, "Unhandled esc[ '%c', %d\n", ch, ch);
        }

        vt->state = VT_STATE_BEGIN;
        vt->dec_private = 0;
        break;
    }
}

static void vt_state_numbersign(struct vt *vt, char ch)
{
    int r, c;
    struct screen_char tmp_e = {
        .chr = 'E',
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    switch (ch) {
    case '8':
        /* Console test feature, entire console is cleared with "E" characters */
        for (r = 0; r < SCR_ROWS; r++)
            for (c = 0; c < SCR_COLS; c++)
                vt->screen->buf[r][c] = tmp_e;
        break;

    default:
        kp(KP_NORMAL, "Unhandled Esc# '%c' (%d)\n", ch, ch);
        vt->state = VT_STATE_BEGIN;
        break;
    }
}


static void vt_state_esc(struct vt *vt, char ch)
{
    switch (ch) {
    case '[':
        vt->state = VT_STATE_LBRACKET;
        vt->esc_param_count = 0;
        memset(vt->esc_params, 0, sizeof(vt->esc_params));
        break;

    case 'M':
        vt_esc_reverse_linefeed(vt, ch);
        vt->state = VT_STATE_BEGIN;
        break;

    case 'D':
        __vt_putchar_nocursor(vt, '\n');
        vt->state = VT_STATE_BEGIN;
        break;

    case 'E':
        __vt_putchar_nocursor(vt, '\r');
        __vt_putchar_nocursor(vt, '\n');
        vt->state = VT_STATE_BEGIN;
        break;

    case '7':
        vt_save_cursor(vt, ch);
        vt->state = VT_STATE_BEGIN;
        break;

    case '8':
        vt_restore_cursor(vt, ch);
        vt->state = VT_STATE_BEGIN;
        break;

    case 'c':
        vt_reset(vt);
        vt->state = VT_STATE_BEGIN;
        break;

    case 'H':
        bit_set(vt->tab_stops, vt->cur_col);
        vt->state = VT_STATE_BEGIN;
        break;

    case '#':
        vt->state = VT_STATE_NUMBERSIGN;
        vt->esc_param_count = 0;
        memset(vt->esc_params, 0, sizeof(vt->esc_params));
        break;

    case '(':
        vt->state = VT_STATE_OPENPAREN;
        break;

    case ')':
        vt->state = VT_STATE_CLOSEPAREN;
        break;

    default:
        kp(KP_NORMAL, "Unexpected char after escape: '%c' (%d)\n", ch, ch);
        vt->state = VT_STATE_BEGIN;
        break;
    }
}

static void vt_state_begin(struct vt *vt, char ch)
{
    if (ch == 27) {
        vt->state = VT_STATE_ESC;
        return;
    }

    __vt_putchar_nocursor(vt, ch);
}

static void vt_state_paren(struct vt *vt, char ch)
{
    /* We don't currently handle switching character set. We still parse the
     * ESC optino though so it doesn't display. We're simply eating the
     * character. */

    vt->state = VT_STATE_BEGIN;
}

static void (*vt_states[]) (struct vt *, char) = {
    [VT_STATE_BEGIN] = vt_state_begin,
    [VT_STATE_ESC] = vt_state_esc,
    [VT_STATE_LBRACKET] = vt_state_lbracket,
    [VT_STATE_NUMBERSIGN] = vt_state_numbersign,
    [VT_STATE_OPENPAREN] = vt_state_paren,
    [VT_STATE_CLOSEPAREN] = vt_state_paren,
};

static void __vt_process_char(struct vt *vt, char ch)
{
    /* The NUL character is ignored */
    if (!ch)
        return;

    if (ch == '\n'
            || ch == '\r'
            || ch == '\t'
            || ch == '\b'
            || ch == '\v'
            || ch == 12)
        return __vt_putchar_nocursor(vt, ch);

    /* We don't handle these control characters, but we shouldn't display them */
    if (ch == 7 || ch == 14 || ch == 15)
        return;

    /* ESC starts a new sequence regardless of what state we were previously in */
    if (ch == 27) {
        vt->state = VT_STATE_ESC;
        return;
    }

    if (ch < 32 && ch >= 0)
        kp(KP_NORMAL, "CTR CHARACTER UNHANDLED: %u\n", (uint8_t)ch);

    if (vt->state < ARRAY_SIZE(vt_states) && vt_states[vt->state])
        (vt_states[vt->state]) (vt, ch);
}

int vt_write(struct vt *vt, const char *buf, size_t len)
{
    using_spinlock(&vt->lock) {
        size_t l;
        for (l = 0; l < len; l++)
            __vt_process_char(vt, buf[l]);

        __vt_updatecur(vt);
    }

    return len;
}

int vt_tty_write(struct tty *tty, const char *buf, size_t len)
{
    struct vt *vt = container_of(tty->driver, struct vt, driver);
    return vt_write(vt, buf, len);
}

void vt_early_init(struct vt *vt)
{
    vt_reset(vt);

    vt->early_init = 1;
}

void vt_init(struct vt *vt)
{
    if (!vt->early_init)
        vt_early_init(vt);

    tty_driver_register(&vt->driver);
}
