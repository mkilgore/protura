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
#include <protura/scheduler.h>
#include <protura/irq.h>
#include <protura/list.h>
#include <protura/work.h>
#include <protura/drivers/tty.h>

#include <arch/spinlock.h>
#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/reset.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/scancode.h>
#include <arch/drivers/keyboard.h>

static struct keyboard {
    uint8_t led_status;
    uint8_t control_keys;

    struct tty *tty;
} keyboard = {
    .led_status = 0,
    .control_keys = 0,
};

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

static void arch_keyboard_interrupt_handler(struct irq_frame *frame, void *param)
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
        char c = asc_char;

        if (keyboard.tty)
            tty_add_input(keyboard.tty, &c, 1);
    }
}

void arch_keyboard_init(void)
{
    irq_register_callback(PIC8259_IRQ0 + 1, arch_keyboard_interrupt_handler, "Keyboard", IRQ_INTERRUPT, NULL);
    pic8259_enable_irq(1);
}

static void clear_keyboard(void)
{
    int test;
    do {
        test = inb(KEY_READ_STATUS_PORT);
        if (test & 0x01)
            inb(KEY_READ_BUF_PORT);
    } while (test & 0x02);
}

void arch_keyboard_set_tty(struct tty *tty)
{
    keyboard.tty = tty;
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
