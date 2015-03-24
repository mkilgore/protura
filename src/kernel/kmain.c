/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/atomic.h>
#include <protura/spinlock.h>
#include <drivers/term.h>
#include <arch/asm.h>
#include <arch/reset.h>
#include <arch/drivers/keyboard.h>
#include <arch/idt.h>

static int get_str(char *buf, size_t len)
{
    int l = 0;

    do {
        hlt();
        if (atomic32_get(&keyboard.has_keys)) {
            int ch;
            while ((ch = keyboard_get_char()) != -1) {

                if (ch == '\n') {
                    term_putchar('\n');
                    buf[l] = '\0';
                    goto end_loop;
                } else if (ch == '\b') {
                    if (l > 0) {
                        l--;
                        /* Erase the previously typed-out character from the screen */
                        term_putchar('\b');
                        term_putchar(' ');
                        term_putchar('\b');
                    }
                } else if (ch < 256) {
                    if (l < len) {
                        term_putchar(ch);
                        buf[l++] = ch;
                    }
                } else if (ch == KEY_UP) {
                    kprintf("UP key\n");
                }
            }
        }
    } while (1);

end_loop:

    return l;
}

int kmain(void)
{
    char cmd_buf[128];
    const char *prompt = "~ # ";

    term_setcurcolor(term_make_color(T_WHITE, T_RED) | 0x08);
    term_printf("\nERROR: Kernel dead-lock. Did you accidentally install Windows ME?\n");
    panic("ERROR: Kernel dead-lock. Did you accidentally install Windows ME?\n");

    term_printf("\nKernel booted!\n");
    kprintf("\nKernel booted!\n");

    while (1) {

        term_setcurcolor(term_make_color(T_RED, T_BLACK));
        term_putstr(prompt);
        term_setcurcolor(term_make_color(T_WHITE, T_BLACK));

        cmd_buf[0] = '\0';
        get_str(cmd_buf, sizeof(cmd_buf));

        if (strcmp(cmd_buf, "interrupts") == 0) {
            interrupt_dump_stats(term_printf);
        } else if (strcmp(cmd_buf, "reboot") == 0) {
            kprintf("KERNEL IS GOING DOWN!!!");
            system_reboot();
        } else {
            term_printf("protura: %s: command not found\n", cmd_buf);
        }
    }

    return 0;
}

