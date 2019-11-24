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
#include <protura/drivers/keyboard.h>

#include <arch/spinlock.h>
#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/reset.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/keyboard.h>

#define KEY_READ_BUF_PORT (0x60)
#define KEY_WRITE_BUF_PORT (0x60)
#define KEY_READ_STATUS_PORT (0x64)
#define KEY_WRITE_CMD_PORT (0x64)

enum {
    X86_LED_NUM_LOCK = 2,
    X86_LED_SCROLL_LOCK = 1,
    X86_LED_CAPS_LOCK = 4,
};

static int led_flags_to_x86(int led_flags)
{
    int ret = 0;

    if (led_flags & KEY_LED_CAPSLOCK)
        ret |= X86_LED_CAPS_LOCK;

    if (led_flags & KEY_LED_SCROLLLOCK)
        ret |= X86_LED_SCROLL_LOCK;

    if (led_flags & KEY_LED_NUMLOCK)
        ret |= X86_LED_NUM_LOCK;

    return ret;
}

void arch_keyboard_set_leds(int led_flags)
{
    int x86_flags = led_flags_to_x86(led_flags);

    outb(KEY_WRITE_BUF_PORT, 0xED);
    while (inb(KEY_READ_STATUS_PORT) & 2)
        ;

    outb(KEY_WRITE_BUF_PORT, x86_flags);
    while (inb(KEY_READ_STATUS_PORT) & 2)
        ;
}

/*
 * From the Linux Kernel source (GPLv2)
 *
 * Converts a keyboard scancode into a keysym for use by the generic keyboard
 * support.
 */
#define E0_BASE 0x60

#define E0_KPENTER (E0_BASE+0)
#define E0_RCTRL   (E0_BASE+1)
#define E0_KPSLASH (E0_BASE+2)
#define E0_PRSCR   (E0_BASE+3)
#define E0_RALT    (E0_BASE+4)
#define E0_BREAK   (E0_BASE+5)  /* (control-pause) */
#define E0_HOME    (E0_BASE+6)
#define E0_UP      (E0_BASE+7)
#define E0_PGUP    (E0_BASE+8)
#define E0_LEFT    (E0_BASE+9)
#define E0_RIGHT   (E0_BASE+10)
#define E0_END     (E0_BASE+11)
#define E0_DOWN    (E0_BASE+12)
#define E0_PGDN    (E0_BASE+13)
#define E0_INS     (E0_BASE+14)
#define E0_DEL     (E0_BASE+15)
/* BTC */
#define E0_MACRO   (E0_BASE+16)
/* LK450 */
#define E0_F13     (E0_BASE+17)
#define E0_F14     (E0_BASE+18)
#define E0_HELP    (E0_BASE+19)
#define E0_DO      (E0_BASE+20)
#define E0_F17     (E0_BASE+21)
#define E0_KPMINPLUS (E0_BASE+22)

#define E1_PAUSE   (E0_BASE+23)

static uint8_t e0_keys[128] = {
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x00-0x07 */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x08-0x0f */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x10-0x17 */
    0, 0, 0, 0, E0_KPENTER, E0_RCTRL, 0, 0,	      /* 0x18-0x1f */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x20-0x27 */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x28-0x2f */
    0, 0, 0, 0, 0, E0_KPSLASH, 0, E0_PRSCR,	      /* 0x30-0x37 */
    E0_RALT, 0, 0, 0, 0, E0_F13, E0_F14, E0_HELP,	      /* 0x38-0x3f */
    E0_DO, E0_F17, 0, 0, 0, 0, E0_BREAK, E0_HOME,	      /* 0x40-0x47 */
    E0_UP, E0_PGUP, 0, E0_LEFT, 0, E0_RIGHT, E0_KPMINPLUS, E0_END,/* 0x48-0x4f */
    E0_DOWN, E0_PGDN, E0_INS, E0_DEL, 0, 0, 0, 0,	      /* 0x50-0x57 */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x58-0x5f */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x60-0x67 */
    0, 0, 0, 0, 0, 0, 0, E0_MACRO,		      /* 0x68-0x6f */
    0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x70-0x77 */
    0, 0, 0, 0, 0, 0, 0, 0			      /* 0x78-0x7f */
};

static uint8_t prev_scancode = 0;

static void arch_keyboard_interrupt_handler(struct irq_frame *frame, void *param)
{
    uint8_t scancode;
    int release_flag = 0;

    scancode = inb(KEY_READ_BUF_PORT);

    if (scancode == 0xE0) {
        prev_scancode = scancode;
        return;
    }

    release_flag = scancode & 0x80;
    scancode = scancode & 0x7F;

    if (prev_scancode == 0xE0) {
        prev_scancode = 0;

        /* This reports the state of the caps-lock and pause key, since those are toggles
         * We handle the state of thsoe keys ourselve, so we ignore these messages */
        if (scancode == 0x2A || scancode == 0x36)
            return;

        if (!e0_keys[scancode]) {
            kp(KP_WARNING, "keyboard: unknown scancode E0 %02x\n", scancode);
            return;
        }

        scancode = e0_keys[scancode];
    } else if (scancode >= E0_BASE) {
        kp(KP_WARNING, "keyboard: scancode not in range 00-%02x\n", E0_BASE - 1);
        return;
    }

    keyboard_submit_keysym(scancode, release_flag);
}

static struct irq_handler keyboard_handler
    = IRQ_HANDLER_INIT(keyboard_handler, "Keyboard", arch_keyboard_interrupt_handler, NULL, IRQ_INTERRUPT, 0);

void arch_keyboard_init(void)
{
    int err = irq_register_handler(1, &keyboard_handler);
    if (err)
        kp(KP_ERROR, "keyboard: Unable to register keyboard interrupt, already registered!\n");
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
