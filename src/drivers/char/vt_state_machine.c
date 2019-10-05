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

#define SCR_DEF_FORGROUND (SCR_WHITE)
#define SCR_DEF_BACKGROUND (SCR_BLACK)

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

static void __vt_clear_color(struct vt *vt, uint8_t color)
{
    int r, c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (r = 0; r < SCR_ROWS; r++)
        for (c = 0; c < SCR_COLS; c++)
            vt->screen->buf[r][c] = tmp;
}

static void __vt_clear_to_end(struct vt *vt)
{
    int r, c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    r = vt->cur_row;
    c = vt->cur_col;

    for (; r < SCR_ROWS; r++, c = 0)
        for (; c < SCR_COLS; c++)
            vt->screen->buf[r][c] = tmp;
}

static void __vt_clear_to_cursor(struct vt *vt)
{
    int r, c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (r = 0; r <= vt->cur_row; r++) {
        int end_c = SCR_COLS;
        if (r == vt->cur_row)
            end_c = vt->cur_col;

        for (c = 0; c < end_c; c++)
            vt->screen->buf[r][c] = tmp;
    }
}

static void __vt_clear_line_to_end(struct vt *vt)
{
    int c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (c = vt->cur_col; c < SCR_COLS; c++)
        vt->screen->buf[vt->cur_row][c] = tmp;
}

static void __vt_clear_line_to_cursor(struct vt *vt)
{
    int c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (c = 0; c < vt->cur_col; c++)
        vt->screen->buf[vt->cur_row][c] = tmp;
}

static void __vt_clear_line(struct vt *vt)
{
    int c;
    struct screen_char tmp = {
        .chr = 32,
        .color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND),
    };

    for (c = 0; c < SCR_COLS; c++)
        vt->screen->buf[vt->cur_row][c] = tmp;
}

void __vt_clear(struct vt *vt)
{
    __vt_clear_color(vt, screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND));
}

void __vt_updatecur(struct vt *vt)
{
    vt->screen->move_cursor(vt->cur_row, vt->cur_col);
}

static void __vt_scroll(struct vt *vt, int lines)
{
    memmove(vt->screen->buf,
            vt->screen->buf[lines],
            SCR_COLS * sizeof(struct screen_char) * (SCR_ROWS - lines));

    struct screen_char tmp;

    tmp.chr = 0;
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    for (c = 0; c < SCR_COLS; c++)
        vt->screen->buf[SCR_ROWS - 1][c] = tmp;
}

static void __vt_scroll_from_cursor(struct vt *vt, int lines)
{
    int start_row = vt->cur_row;

    memmove(vt->screen->buf[start_row],
            vt->screen->buf[start_row + lines],
            SCR_COLS * sizeof(struct screen_char) * (SCR_ROWS - lines - start_row));

    struct screen_char tmp;

    tmp.chr = 0;
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    int r;
    for (r = SCR_ROWS - lines; r < SCR_ROWS; r++)
        for (c = 0; c < SCR_COLS; c++)
            vt->screen->buf[r][c] = tmp;
}

static void __vt_scroll_up_from_cursor(struct vt *vt, int lines)
{
    int start_row = vt->cur_row;

    memmove(vt->screen->buf[start_row + lines],
            vt->screen->buf[start_row],
            SCR_COLS * sizeof(struct screen_char) * (SCR_ROWS - lines - start_row));

    struct screen_char tmp;

    tmp.chr = 0;
    tmp.color = screen_make_color(SCR_DEF_FORGROUND, SCR_DEF_BACKGROUND);

    int c;
    int r;
    for (r = start_row; r < start_row + lines; r++)
        for (c = 0; c < SCR_COLS; c++)
            vt->screen->buf[r][c] = tmp;
}

static void __vt_putchar_nocur(struct vt *vt, char ch)
{
    uint8_t r = vt->cur_row;
    uint8_t c = vt->cur_col;

    /* We do this up here so that ignore_next_nl gets cleared for every other
     * char. */
    if (ch == '\r')
        c = 0;

    if (ch == '\n' && !vt->ignore_next_nl) {
        c = 0;
        r++;
    } else {
        vt->ignore_next_nl = 0;
    }

    switch (ch) {
    case '\n':
    case '\r':
        break;

    case '\t':
        if ((c % 8) == 0)
            c += 8;
        c += 8 - (c % 8);
        break;

    case '\b':
        if (c > 0)
            c--;
        vt->screen->buf[r][c].color = vt_create_cur_color(vt);
        vt->screen->buf[r][c].chr = ' ';
        break;

    /* Some special codes like newline, tab, backspace, etc. are
     * handled above. The rest are printed in ^x format. */
    case '\x01': /* ^A to ^] */
    case '\x02':
    case '\x03':
    case '\x04':
    case '\x05':
    case '\x06':
    case '\x07':
    case '\x0B':
    case '\x0C':
    case '\x0E':
    case '\x0F':
    case '\x10':
    case '\x11':
    case '\x12':
    case '\x13':
    case '\x14':
    case '\x15':
    case '\x16':
    case '\x17':
    case '\x18':
    case '\x19':
    case '\x1A':
    case '\x1B':
        __vt_putchar_nocur(vt, '^');
        __vt_putchar_nocur(vt, ch + 'A' - 1);
        return ;

    default:
        vt->screen->buf[r][c].color = vt_create_cur_color(vt);
        vt->screen->buf[r][c].chr = ch;
        c++;
        if (c >= SCR_COLS) {
            vt->ignore_next_nl = 1;
            c = 0;
            r++;
        }
        break;
    }

    if (r >= SCR_ROWS) {
        __vt_scroll(vt, 1);
        r--;
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
    if (new_row < 0)
        vt->cur_row = 0;
    else if (new_row > SCR_ROWS - 1)
        vt->cur_row = SCR_ROWS - 1;
    else
        vt->cur_row = new_row;

    if (new_col < 0)
        vt->cur_col = 0;
    else if (new_col > SCR_COLS - 1)
        vt->cur_col = SCR_COLS - 1;
    else
        vt->cur_col = new_col;

    vt->ignore_next_nl = 0;
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

    __vt_set_cursor(vt, new_row, new_col);
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

static void vt_tty_write_string(struct tty *tty, const char *str)
{
    tty_add_input(tty, str, strlen(str));
}

static void vt_esc_report(struct vt *vt, char cmd)
{
    char buf[32];

    switch (vt->esc_params[0]) {
    case 6:
        snprintf(buf, sizeof(buf), "\033[%d;%dR", vt->cur_row + 1, vt->cur_col + 1);
        return vt_tty_write_string(vt->tty, buf);

    case 7:
        return vt_tty_write_string(vt->tty, "\033[0n");
    }
}

static void vt_esc_set_mode(struct vt *vt, char cmd)
{
    int mode_on = 1;
    if (cmd == 'l')
        mode_on = 0;

    if (!vt->dec_private)
        return;

    switch (vt->esc_params[0]) {
    case 25:
        if (mode_on)
            (vt->screen->cursor_on) ();
        else
            (vt->screen->cursor_off) ();
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

static void vt_esc_insert_line(struct vt *vt, char cmd)
{
    int lines = 1;
    if (vt->esc_params[0])
        lines = vt->esc_params[0];

    __vt_scroll_up_from_cursor(vt, lines);
}

static void (*lbracket_table[256]) (struct vt *, char) = {
    ['m'] = vt_esc_set_attributes,
    ['H'] = vt_esc_cursor_move,
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
    ['L'] = vt_esc_insert_line,
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

        if (lbracket_table[(int)ch])
            (lbracket_table[(int)ch]) (vt, ch);
        else {
            kp(KP_NORMAL, "Unhandled esc[ '%c', %d\n", ch, ch);
            if (vt->dec_private) {
                int i;
                kp(KP_NORMAL, "DEC PRIVATE mode: '%c', %d\n", ch, ch);
                for (i = 0; i < vt->esc_param_count; i++)
                    kp(KP_NORMAL, "    ARG: %d\n", vt->esc_params[i]);
            }
        }

        vt->state = VT_STATE_BEGIN;
        vt->dec_private = 0;
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

    default:
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

    __vt_putchar_nocur(vt, ch);
}

static void (*vt_states[]) (struct vt *, char) = {
    [VT_STATE_BEGIN] = vt_state_begin,
    [VT_STATE_ESC] = vt_state_esc,
    [VT_STATE_LBRACKET] = vt_state_lbracket,
};

static void __vt_process_char(struct vt *vt, char ch)
{
    if (ch == '\n'
            || ch == '\r'
            || ch == '\t'
            || ch == '\b')
        return __vt_putchar_nocur(vt, ch);

    /* We don't handle these control characters, but we shouldn't display them */
    if (ch == 7 || ch == 14 || ch == 15)
        return;

    if (vt->state < ARRAY_SIZE(vt_states) && vt_states[vt->state])
        (vt_states[vt->state]) (vt, ch);
}

int vt_write(struct vt *vt, const char *buf, size_t len)
{
    scoped_spinlock(&vt->lock) {
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
    arch_screen_init();

    vt->fg_color = SCR_DEF_FORGROUND;
    vt->bg_color = SCR_DEF_BACKGROUND;
    vt->cur_attrs = 0;

    __vt_clear(vt);
    __vt_updatecur(vt);

    vt->early_init = 1;
}

void vt_init(struct vt *vt)
{
    if (!vt->early_init)
        vt_early_init(vt);

    // screen_init();
    keyboard_init();

    tty_driver_register(&vt->driver);
}
