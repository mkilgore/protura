/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/bits.h>
#include <protura/debug.h>
#include <protura/atomic.h>
#include <protura/spinlock.h>

#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/reset.h>
#include <arch/scheduler.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/scancode.h>
#include <arch/drivers/keyboard.h>

struct keyboard keyboard = {
    .led_status = 0,
    .control_keys = 0,

    .has_keys = ATOMIC32_INIT(0),
    .buf_lock = SPINLOCK_INIT("Keyboard"),
};

#define toupper(c) ((c >= 'a' && c <= 'z')? (c | 32): c)

#define KEY_READ_BUF_PORT (0x60)
#define KEY_WRITE_BUF_PORT (0x60)
#define KEY_READ_STATUS_PORT (0x64)
#define KEY_WRITE_CMD_PORT (0x64)

static void set_leds(void)
{
    outb(KEY_WRITE_BUF_PORT, 0xED);
    while (inb(KEY_READ_STATUS_PORT) & 2)
        ;
    outb(KEY_WRITE_BUF_PORT, keyboard.led_status);
    while (inb(KEY_READ_STATUS_PORT) & 2)
        ;
}

int keyboard_has_char(void)
{
    if (atomic32_get(&keyboard.has_keys))
        return 1;
    else
        return 0;
}

int keyboard_get_char(void)
{
    short ch;
    if (!atomic32_get(&keyboard.has_keys))
        return -1;

    using_spinlock(&keyboard.buf_lock)
        ch = char_buf_read_char(&keyboard.buf);

    atomic32_dec(&keyboard.has_keys);
    return ch;
}

static void keyboard_interrupt_handler(struct idt_frame *frame)
{
    int scancode;
    short asc_char = 0;

    scancode = inb(KEY_READ_BUF_PORT);

    if (scancode == KEY_CAPS_LOCK) {
        keyboard.led_status ^= LED_CAPS_LOCK;
        set_leds();
    } else if (scancode == KEY_NUM_LOCK) {
        keyboard.led_status ^= LED_NUM_LOCK;
        set_leds();
    } else if (scancode == KEY_SCROLL_LOCK) {
        keyboard.led_status ^= LED_SCROLL_LOCK;
        set_leds();
    }

    if (scancode == KEY_CONTROL)
        keyboard.control_keys |= CK_CTRL;

    if (scancode == (0x80 + KEY_CONTROL))
        keyboard.control_keys &= ~CK_CTRL;

    if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT)
        keyboard.control_keys |= CK_SHIFT;

    if (scancode == (0x80 + KEY_LEFT_SHIFT) || scancode == (0x80 + KEY_RIGHT_SHIFT))
        keyboard.control_keys &= ~CK_SHIFT;

    if (scancode == KEY_ALT)
        keyboard.control_keys |= CK_ALT;

    if (scancode == (0x80 + KEY_ALT))
        keyboard.control_keys &= ~CK_ALT;

    if (scancode & 0x80)
        return ;

    if ((keyboard.control_keys & CK_SHIFT) && (keyboard.led_status & LED_CAPS_LOCK))
        asc_char = scancode2_to_ascii[scancode][6];
    else if (keyboard.control_keys & CK_SHIFT)
        asc_char = scancode2_to_ascii[scancode][1];
    else if (keyboard.control_keys & CK_CTRL)
        asc_char = scancode2_to_ascii[scancode][2];
    else if (keyboard.control_keys & CK_ALT)
        asc_char = scancode2_to_ascii[scancode][3];
    else if ((keyboard.control_keys & CK_SHIFT) && (keyboard.led_status & LED_NUM_LOCK))
        asc_char = scancode2_to_ascii[scancode][7];
    else if (keyboard.led_status & LED_CAPS_LOCK)
        asc_char = scancode2_to_ascii[scancode][5];
    else if (keyboard.led_status & LED_NUM_LOCK)
        asc_char = scancode2_to_ascii[scancode][4];
    else if (keyboard.control_keys == 0)
        asc_char = scancode2_to_ascii[scancode][0];

    if (asc_char) {
        using_spinlock(&keyboard.buf_lock) {
            char_buf_write_char(&keyboard.buf, asc_char);
            atomic32_inc(&keyboard.has_keys);

            wakeup_list_wakeup(&keyboard.watch_list);
        }
    }
}

void keyboard_init(void)
{
    kprintf("Enabling keyboard\n");
    char_buf_init(&keyboard.buf, keyboard.buffer, sizeof(keyboard.buffer));
    wakeup_list_init(&keyboard.watch_list);

    irq_register_callback(PIC8259_IRQ0 + 1, keyboard_interrupt_handler, "Keyboard", IRQ_INTERRUPT);
    pic8259_enable_irq(1);

    return ;
}

void clear_keyboard(void)
{
    int test;
    do {
        test = inb(KEY_READ_STATUS_PORT);
        if (test & 0x01)
            inb(KEY_READ_BUF_PORT);
    } while (test & 0x02);
}

void keyboard_wakeup_add(struct task *t)
{
    wakeup_list_add(&keyboard.watch_list, t);
}

void keyboard_wakeup_remove(struct task *t)
{
    wakeup_list_remove(&keyboard.watch_list, t);
}

/* A fake IDT setup - Issuing an interrupt with this IDT will triple-fault the
 * CPU, causing a reset */
static struct idt_ptr fake_idt_ptr = { 0, 0 };

void system_reboot(void)
{
    cli();

    clear_keyboard();

    outb(KEY_WRITE_CMD_PORT, 0xFE); /* CPU reset */

    idt_flush((uintptr_t)&fake_idt_ptr);

    /* Deref an address in page zero, resulting in a page fault
     * With an empty idt, this should triple fault, a less gracefull way of
     * rebooting. */
    *((volatile int *)0x42) = 20;

    while (1)
        hlt(); /* Uhh, if we're here just give up */
}

void system_shutdown(void)
{
    cli();
    while (1)
        hlt();
}

