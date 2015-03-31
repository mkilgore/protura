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
#include <arch/task.h>
#include <arch/cpu.h>
#include <arch/idt.h>
#include <arch/init.h>

int kernel_is_booting = 1;

void kmain(void)
{
    struct sys_init *sys;
    struct task *first;

    kprintf("VFS init\n");
    vfs_init();

    kprintf("Seting up task switching\n");
    task_init();

    first = task_fake_create();
    task_fake_create();
    task_fake_create();
    task_fake_create();

    /* Loop through things to initalize and start them (timer, keyboard, etc...). */
    for (sys = arch_init_systems; sys->name; sys++) {
        kprintf("Starting: %s\n", sys->name);
        (sys->init) ();
    }

    kprintf("Kernel is done booting!\n");

    kernel_is_booting = 0;

    cpu_get_local()->current = first;

    task_start_init();

    panic("ERROR! task_start_init() returned!\n");
    while (1)
        hlt();
}

