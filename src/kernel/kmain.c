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
#include <protura/strtol.h>
#include <drivers/term.h>
#include <arch/asm.h>
#include <arch/reset.h>
#include <arch/drivers/keyboard.h>
#include <arch/task.h>
#include <arch/scheduler.h>
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

    term_printf("ID: %s\n", argv[0]);

    if (argc == 2)
        id = strtol(argv[1], NULL, 0);

    term_printf("Id: %d\n", id);

    while (1) {
        term_printf("%d ", id);
        scheduler_task_sleep(1550);
    }
}

int kernel_keyboard_thread(int argc, const char **argv)
{
    kprintf("Keyboard watch task started!\n");
    keyboard_wakeup_add(cpu_get_local()->current);
    while (1) {
        /* Go back to sleep until the keyboard wakes us up */
        cpu_get_local()->current->state = TASK_SLEEPING;
        scheduler_task_yield();

        /* If we got here, then we were woke up by the keyboard */
        term_printf("Task started by keyboard presses: ");
        char ch;

        while ((ch = keyboard_get_char()) != -1)
            term_putchar(ch);

        term_printf("\n");
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

    scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create());

    /* task_schedule_add(task_kernel_new_interruptable("Kernel thread 1", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "1" }));
    task_schedule_add(task_kernel_new_interruptable("Kernel thread 2", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "2" }));
    task_schedule_add(task_kernel_new_interruptable("Kernel thread 3", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "3" }));
    task_schedule_add(task_kernel_new_interruptable("Kernel thread 4", test_kernel_thread, 2, (const char *[]){ "Kernel thread", "4" })); */
    scheduler_task_add(task_kernel_new("Keyboard watch", kernel_keyboard_thread, 0, (const char *[]) { }));

    cpu_start_scheduler();

    panic("ERROR! cpu_start_scheduler() returned!\n");
    while (1)
        hlt();
}

