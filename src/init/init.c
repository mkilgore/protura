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
#include <protura/cmdline.h>
#include <protura/mm/kmalloc.h>
#include <arch/init.h>
#include <arch/asm.h>
#include <protura/fs/fs.h>
#include <protura/fs/namei.h>
#include <protura/fs/vfs.h>
#include <protura/drivers/console.h>
#include <protura/ktest.h>

#include <protura/init/init_basic.h>

/* Initial user task */
struct task *task_pid1;

int kernel_is_booting = 1;

static int start_user_init(void *unused)
{
    struct sys_init *sys;

    /* Loop through set of initialiations that run after the scheduler has started (most of them) */
    for (sys = arch_init_systems; sys->name; sys++) {
        kp(KP_NORMAL, "Starting: %s\n", sys->name);
        (sys->init) ();
    }

    /* Mount the current IDE drive as an ext2 filesystem */
    int ret = mount_root(DEV_MAKE(CONFIG_ROOT_MAJOR, CONFIG_ROOT_MINOR), CONFIG_ROOT_FSTYPE);
    if (ret)
        panic("UNABLE TO MOUNT ROOT FILESYSTEM\n");

#ifdef CONFIG_KERNEL_TESTS
    ktest_init();
#endif

    kp(KP_NORMAL, "Kernel is done booting!\n");

    vt_console_kp_unregister();

    const char *init_prog = kernel_cmdline_get_string("init", "/bin/init");

    kp(KP_NORMAL, "Starting \"%s\"...\n", init_prog);

    task_pid1 = task_user_new_exec(init_prog);
    task_pid1->pid = 1;

    scheduler_task_add(task_pid1);

    return 0;
}

/* 
 * We want to get to a process context as soon as possible, as not being in one
 * complicates what we can and can't do (For example, cpu_get_local()->current
 * is NULL until we enter a process context, so we can't sleep and we can't
 * register for wait queues, take mutex's, etc...). This is similar to an
 * interrupt context.
 */
void kmain(void)
{
    reboot_on_panic  = kernel_cmdline_get_bool("reboot_on_panic", 0);

    cpu_setup_idle();

    struct task *t = kmalloc(sizeof(*t), PAL_KERNEL | PAL_ATOMIC);
    if (!t)
        panic("Unable to allocate kernel init task!\n");

    task_init(t);
    task_kernel_generic(t, "Kernel init", start_user_init, NULL, 1);
    scheduler_task_add(t);

    kp(KP_NORMAL, "Starting scheduler\n");
    cpu_start_scheduler();

    panic("ERROR! cpu_start_scheduler() returned!\n");
    while (1)
        hlt();
}

