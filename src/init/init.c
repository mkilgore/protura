/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/stddef.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <arch/init.h>
#include <arch/asm.h>

#include <init/init_task.h>
#include <init/init_basic.h>


int kernel_is_booting = 1;

/* 
 * We want to get to a process context as soon as possible, as not being in one
 * complicates what we can and can't do (For example, cpu_get_local()->current
 * is NULL until we enter a process context, so we can't sleep and we can't
 * register for wait queues, take mutex's, etc...). This is similar to an
 * interrupt context.
 *
 * To achieve this, there is a statically allocated struct task called
 * 'init_task', which we assign to 'current'.
 */
void kmain(void)
{
    struct sys_init *sys;
    /*
    kprintf("Seting up task switching\n");
    task_init_start(init_second_half, NULL); */

    cpu_get_local()->current = &init_task;

    cpu_setup_idle();

    /* Loop through things to initalize and start them (timer, keyboard, etc...). */
    for (sys = arch_init_systems; sys->name; sys++) {
        kprintf("Starting: %s\n", sys->name);
        (sys->init) ();
    }

    kprintf("Kernel is done booting!\n");
    kernel_is_booting = 0;

    scheduler_task_add(task_kernel_new_interruptable("Keyboard watch", kernel_keyboard_thread, NULL));

    kprintf("Starting scheduler\n");
    cpu_start_scheduler();

    panic("ERROR! cpu_start_scheduler() returned!\n");
    while (1)
        hlt();
}

