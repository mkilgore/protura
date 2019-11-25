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
#include <protura/scheduler.h>
#include <protura/signal.h>
#include <protura/wait.h>
#include <protura/scheduler.h>
#include <protura/drivers/tty.h>

#include <arch/spinlock.h>
#include <arch/drivers/keyboard.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/console.h>

static struct keyboard {
    uint8_t led_status;

    uint16_t mod_flags;
    uint8_t mod_count[KEY_MOD_MAX];

    struct tty *tty;
} keyboard = {
    .led_status = 0,
    .mod_flags = 0,
};

static void handle_null(uint8_t keysym, int release_flag)
{
}

/* keysym is an ascii character */
static void handle_reg(uint8_t keysym, int release_flag)
{
    if (release_flag)
        return;

    struct tty *tty = READ_ONCE(keyboard.tty);
    if (tty)
        tty_add_input(tty, (char *)&keysym, 1);
}

/* keysum is a KEY_LED_* flag */
static void handle_led_key(uint8_t keysym, int release_flag)
{
    if (release_flag)
        return;

    keyboard.led_status ^= F(keysym);
}

/* keysum is a KEY_MOD_* flag */
static void handle_mod(uint8_t keysym, int release_flag)
{
    if (release_flag) {
        if (keyboard.mod_count[keysym])
            keyboard.mod_count[keysym]--;
    } else {
        keyboard.mod_count[keysym]++;
    }

    if (keyboard.mod_count[keysym])
        keyboard.mod_flags |= F(keysym);
    else
        keyboard.mod_flags &= ~F(keysym);
}

/* keysum is a KEY_CURSOR_* flag */
static void handle_cursor(uint8_t keysym, int release_flag)
{
    if (release_flag)
        return;

    if (keysym > 3)
        return;

    char buf[4] = { 27, '[', 0 };
    buf[2] = "ABDC"[keysym];

    struct tty *tty = READ_ONCE(keyboard.tty);
    if (tty)
        tty_add_input(tty, buf, 3);
}

/* keysum is an index into the string array */
static void handle_str(uint8_t keysym, int release_flag)
{
    if (release_flag || keysym >= KEY_STR_MAX)
        return;

    struct tty *tty = READ_ONCE(keyboard.tty);
    if (tty)
        tty_add_input_str(tty, keycode_str_table[keysym]);
}

static void handle_console(uint8_t keysym, int release_flag)
{
    if (release_flag)
        return;

    int new_console = keysym;
    console_switch_vt(new_console);
}

static const char *pad_num_strs[KEY_PAD_MAX] = {
    [KEY_PAD_SEVEN]  = "7",
    [KEY_PAD_EIGHT]  = "8",
    [KEY_PAD_NINE]   = "9",
    [KEY_PAD_MINUS]  = "-",
    [KEY_PAD_FOUR]   = "4",
    [KEY_PAD_FIVE]   = "5",
    [KEY_PAD_SIX]    = "6",
    [KEY_PAD_PLUS]   = "+",
    [KEY_PAD_ONE]    = "1",
    [KEY_PAD_TWO]    = "2",
    [KEY_PAD_THREE]  = "3",
    [KEY_PAD_ZERO]   = "0",
    [KEY_PAD_PERIOD] = ".",
    [KEY_PAD_ENTER]  = "\n",
    [KEY_PAD_SLASH]  = "/",
};

/* keysum is a KEY_PAD_* value */
static void handle_pad(uint8_t keysym, int release_flag)
{
    if (release_flag)
        return;

    struct tty *tty = READ_ONCE(keyboard.tty);
    if (!tty)
        return;

    if (keysym >= KEY_PAD_MAX)
        return;

    if (flag_test(&keyboard.led_status, KEY_LED_NUMLOCK)) {
        tty_add_input_str(tty, pad_num_strs[keysym]);
    } else {
        switch (keysym) {
        case KEY_PAD_SEVEN:  return handle_str(KEY_STR_HOME, release_flag);
        case KEY_PAD_EIGHT:  return handle_cursor(KEY_CUR_UP, release_flag);
        case KEY_PAD_NINE:   return handle_str(KEY_STR_PAGE_UP, release_flag);
        case KEY_PAD_MINUS:  return;
        case KEY_PAD_FOUR:   return handle_cursor(KEY_CUR_LEFT, release_flag);
        case KEY_PAD_FIVE:   return;
        case KEY_PAD_SIX:    return handle_cursor(KEY_CUR_RIGHT, release_flag);
        case KEY_PAD_PLUS:   return;
        case KEY_PAD_ONE:    return handle_str(KEY_STR_END, release_flag);
        case KEY_PAD_TWO:    return handle_cursor(KEY_CUR_DOWN, release_flag);
        case KEY_PAD_THREE:  return handle_str(KEY_STR_PAGE_DOWN, release_flag);
        case KEY_PAD_ZERO:   return handle_str(KEY_STR_INSERT, release_flag);
        case KEY_PAD_PERIOD: return handle_str(KEY_STR_DELETE, release_flag);
        case KEY_PAD_ENTER:  return;
        case KEY_PAD_SLASH:  return;
        }
    }
}

static void (*key_handler[KT_MAX]) (uint8_t, int) = {
    [KT_NULL] = handle_null,
    [KT_REG] = handle_reg,
    [KT_LETTER] = handle_null, /* Should not happen */
    [KT_LED_KEY] = handle_led_key,
    [KT_MOD] = handle_mod,
    [KT_CURSOR] = handle_cursor,
    [KT_STR] = handle_str,
    [KT_PAD] = handle_pad,
    [KT_CONSOLE] = handle_console,
};

void keyboard_submit_keysym(uint8_t keysym, int release_flag)
{
    uint8_t table = keyboard.mod_flags;

    if (table >= F(KEY_MOD_MAX) || !keycode_maps[table])
        return;

    struct keycode key = keycode_maps[table][keysym];

    /* Handle caps lock transformation if necessary */
    if (key.type == KT_LETTER) {
        if (flag_test(&keyboard.led_status, KEY_LED_CAPSLOCK)) {
            table = table ^ F(KEY_MOD_SHIFT);

            if (keycode_maps[table])
                key = keycode_maps[table][keysym];
        }

        key.type = KT_REG;
    }

    if (key.type < KT_MAX && key_handler[key.type])
        (key_handler[key.type]) (key.code, release_flag);
}

void keyboard_set_tty(struct tty *tty)
{
    WRITE_ONCE(keyboard.tty, tty);
}

void keyboard_init(void)
{
    arch_keyboard_init();
}
