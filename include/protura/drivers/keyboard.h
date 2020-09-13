/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_KEYBOARD_H
#define INCLUDE_DRIVERS_KEYBOARD_H

#include <protura/types.h>
#include <protura/fs/char.h>
#include <protura/event/keyboard.h>

struct tty;

int keyboard_file_read(struct file *filp, void *vbuf, size_t len);

void keyboard_submit_keysym(uint8_t keysym, int release_flag);
void keyboard_set_tty(struct tty *tty);
void keyboard_set_state(int state);

extern struct file_ops keyboard_file_ops;

enum keytype {
    KT_NULL,

    /* "Regular" characters/ascii values */
    KT_REG,

    /* Characters affected by shift/caps-lock */
    KT_LETTER,

    /* Caps-Lock, Scroll-Lock, Num-Lock */
    KT_LED_KEY,

    /* Shift, Ctrl, Alt */
    KT_MOD,

    /* Cursor keys */
    KT_CURSOR,

    /* Output string, index into str table */
    KT_STR,

    /* A Keypad character */
    KT_PAD,

    /* Switch to new console */
    KT_CONSOLE,

    KT_MAX
};

enum kt_mod_type {
    KEY_MOD_SHIFT,
    KEY_MOD_CTRL,
    KEY_MOD_ALT,
    KEY_MOD_MAX
};

enum kt_led_type {
    KEY_LED_CAPSLOCK,
    KEY_LED_NUMLOCK,
    KEY_LED_SCROLLLOCK,
};

enum kt_cursor_type {
    KEY_CUR_UP,
    KEY_CUR_DOWN,
    KEY_CUR_LEFT,
    KEY_CUR_RIGHT,
};

enum kt_pad_type {
    KEY_PAD_SEVEN,
    KEY_PAD_EIGHT,
    KEY_PAD_NINE,
    KEY_PAD_MINUS,
    KEY_PAD_FOUR,
    KEY_PAD_FIVE,
    KEY_PAD_SIX,
    KEY_PAD_PLUS,
    KEY_PAD_ONE,
    KEY_PAD_TWO,
    KEY_PAD_THREE,
    KEY_PAD_ZERO,
    KEY_PAD_PERIOD,
    KEY_PAD_ENTER,
    KEY_PAD_SLASH,
    KEY_PAD_MAX,
};

enum kt_str_type {
    KEY_STR_F1,
    KEY_STR_F2,
    KEY_STR_F3,
    KEY_STR_F4,
    KEY_STR_F5,
    KEY_STR_F6,
    KEY_STR_F7,
    KEY_STR_F8,
    KEY_STR_F9,
    KEY_STR_F10,
    KEY_STR_F11,
    KEY_STR_F12,
    KEY_STR_INSERT,
    KEY_STR_DELETE,
    KEY_STR_HOME,
    KEY_STR_END,
    KEY_STR_PAGE_UP,
    KEY_STR_PAGE_DOWN,
    KEY_STR_MAX,
};

extern const char *keycode_str_table[KEY_STR_MAX];

struct keycode {
    uint8_t type;
    uint8_t code;
};

/* This is indexed by the KEY_SHIFT_* flags */
extern struct keycode *keycode_maps[8];

extern const char *keycode_str_table[];

#define KC(t, c) \
    { \
        .type = (t), \
        .code = (c), \
    }

#endif
