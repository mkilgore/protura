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
#include <protura/kassert.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <protura/drivers/term.h>
#include <arch/drivers/keyboard.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/console.h>

void keyboard_init(void)
{
    arch_keyboard_init();
    return ;
}

static int keyboard_file_read(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    int c, cur_pos = 0;
    struct task *current = cpu_get_local()->current;

    kp(KP_TRACE, "Registering %d for keyboard\n", current->pid);
    arch_keyboard_wakeup_add(current);

  sleep_again:
    sleep {
        while ((cur_pos != len && (c = arch_keyboard_has_char()))) {
            buf[cur_pos] = arch_keyboard_get_char();
            cur_pos++;
        }

        /* Only sleep if there was no data to be gotten */
        if (!cur_pos) {
            scheduler_task_yield();
            goto sleep_again;
        }
    }

    arch_keyboard_wakeup_remove(current);
    kp(KP_TRACE, "Unregistering %d for keyboard\n", current->pid);

    return cur_pos;
}

struct file_ops keyboard_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = keyboard_file_read,
    .write = NULL,
    .lseek = NULL,
    .readdir = NULL,
};

