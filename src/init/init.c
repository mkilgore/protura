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
#include <protura/task.h>
#include <arch/init.h>
#include <arch/asm.h>
#include <fs/fs.h>
#include <drivers/term.h>

#include <init/init_task.h>
#include <init/init_basic.h>


int kernel_is_booting = 1;

static int start_user_init(void *unused)
{
    /* Mount the current IDE drive as an ext2 filesystem */
    sb_root = (file_system_lookup("ext2")->read_sb) (DEV_MAKE(BLOCK_DEV_IDE, 0));
    ino_root = sb_root->root;

    struct task *user_init;

    user_init = task_user_new_exec("/init");
    user_init->pid = 1;
    scheduler_task_add(user_init);

    /* sys_exit(0); */
    return 0;
}

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

    cpu_get_local()->current = &init_task;

    cpu_setup_idle();

    /* Loop through things to initalize and start them (timer, keyboard, etc...). */
    for (sys = arch_init_systems; sys->name; sys++) {
        kp(KP_NORMAL, "Starting: %s\n", sys->name);
        (sys->init) ();
    }

    kp(KP_NORMAL, "Kernel is done booting!\n");

    kp_output_unregister(term_printfv);

    scheduler_task_add(task_kernel_new("User Init Bootstrap", start_user_init, NULL));

    /*
    scheduler_task_add(task_kernel_new_interruptable("Keyboard watch", kernel_keyboard_thread, NULL));
    */

    kp(KP_NORMAL, "Starting scheduler\n");
    cpu_start_scheduler();

    panic("ERROR! cpu_start_scheduler() returned!\n");
    while (1)
        hlt();
}

