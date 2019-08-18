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

int keyboard_file_read(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    size_t c, cur_pos = 0;
    int ret = 0;
    struct task *current = cpu_get_local()->current;
    struct work keyboard_work = WORK_INIT_TASK(keyboard_work, current);

    kp(KP_TRACE, "Registering %d for keyboard\n", current->pid);
    arch_keyboard_work_add(&keyboard_work);

    while (1) {
        while ((cur_pos != len && (c = arch_keyboard_has_char()))) {
            buf[cur_pos] = arch_keyboard_get_char();
            cur_pos++;
        }

        if (cur_pos == len)
            break;

        ret = sleep_event_intr(arch_keyboard_has_char());
        if (ret)
            goto cleanup;
    }

    ret = cur_pos;

  cleanup:
    arch_keyboard_work_remove(&keyboard_work);
    kp(KP_TRACE, "Unregistering %d for keyboard\n", current->pid);

    return ret;
}

struct file_ops keyboard_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = keyboard_file_read,
    .write = NULL,
    .lseek = NULL,
    .readdir = NULL,
};

