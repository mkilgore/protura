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
#include <arch/backtrace.h>

int kernel_is_booting = 1;

int test_kernel_thread(int argc, const char **argv)
{
    int id = 0;
    term_printf("In kernel thread!!!\n");

    term_printf("Argc: %d\n", argc);
    term_printf("Argv: %p\n", argv);

    if (argc == 2)
        id = argv[1][0] - '0';

    term_printf("Id: %d\n", id);

    while (1) {
        term_printf("Yielding %d...\n", id);
        task_yield();
    }
}

void kmain(void)
{
    struct sys_init *sys;

    kprintf("VFS init\n");
    vfs_init();

    kprintf("Seting up task switching\n");
    task_init();


    /* Loop through things to initalize and start them (timer, keyboard, etc...). */
    for (sys = arch_init_systems; sys->name; sys++) {
        kprintf("Starting: %s\n", sys->name);
        (sys->init) ();
    }

    kprintf("Kernel is done booting!\n");

    kernel_is_booting = 0;

    task_fake_create();
    task_fake_create();

    task_kernel_new_interruptable("Kernel thread 1", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "2" });
    task_kernel_new_interruptable("Kernel thread 2", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "3" });
    task_kernel_new_interruptable("Kernel thread 3", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "4" });
    task_kernel_new_interruptable("Kernel thread 4", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "5" });

    scheduler();

    panic("ERROR! task_start_init() returned!\n");
    while (1)
        hlt();
}

