/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/stddef.h>
#include <protura/string.h>
#include <protura/atomic.h>
#include <protura/spinlock.h>
#include <protura/strtol.h>
#include <protura/dump_mem.h>
#include <mm/kmalloc.h>
#include <drivers/term.h>
#include <drivers/ide.h>
#include <arch/asm.h>
#include <arch/reset.h>
#include <arch/drivers/keyboard.h>
#include <arch/task.h>
#include <arch/scheduler.h>
#include <arch/cpu.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/backtrace.h>
#include <fs/simple_fs.h>

int kernel_is_booting = 1;

void sleep_for_keyboard(void)
{
  sleep_again:
    sleep {
        if (!keyboard_has_char()) {
            scheduler_task_yield();
            goto sleep_again;
        }
    }
}

void read_sector(char *buf, ksector_t sector)
{
    struct block *b;
    kprintf("Starting IDE test!\n");

    b = bread(DEV_MAKE(DEV_IDE, 0), sector);
    memcpy(buf, b->data, b->block_size);

    b->data[0] = '\xFF';

    block_mark_dirty(b);

    brelease(b);
}

static char output[80 * 512 / 16 + 1];

int kernel_keyboard_thread(int argc, const char **argv)
{
    char sector[512];
    char cmd_buf[200];
    int len = 0;
    ksector_t s;

    kprintf("Keyboard watch task started!\n");
    keyboard_wakeup_add(cpu_get_local()->current);

    struct super_block *sb = simple_fs_read_sb(DEV_MAKE(DEV_IDE, 0));
    struct simple_fs_super_block *simple = container_of(sb, struct simple_fs_super_block, sb);
    struct simple_fs_inode *inode;
    struct block *b;
    int files;

    kprintf("Simple: %p\n", simple);
    kprintf("Root: %p\n", simple->sb.root);
    kprintf("Root ino: %d\n", simple->sb.root->ino);

    inode = container_of(simple->sb.root, struct simple_fs_inode, i);
    kprintf("Root data sector: %d\n", inode->contents[0]);

    files = inode->i.size / sizeof(struct kdirent);

    using_block(simple->sb.dev, inode->contents[0], b) {
        struct kdirent *ents = (struct kdirent *)b->data;
        int i;

        for (i = 0; i < files; i++)
            kprintf("File: %s, Inode: %d\n", ents[i].name, ents[i].ino);
    }

    sb->ops->put_sb(sb);

    term_printf("Sector: ");

    while (1) {
        int ch;

        sleep_for_keyboard();

        while ((ch = keyboard_get_char()) != -1) {
            term_putchar(ch);
            switch (ch) {
            case '\n':
                cmd_buf[len] = '\0';
                s = strtol(cmd_buf, NULL, 0);

                term_printf("Reading sector %d\n", s);
                read_sector(sector, s);

                output[0] = '\0';

                dump_mem(output, sizeof(output), sector, sizeof(sector), s * 512);

                term_printf("%s", output);
                kprintf("%s", output);

                if (s == 22)
                    block_cache_sync();

                len = 0;
                term_printf("Sector: ");
                break;

            default:
                if (len < sizeof(cmd_buf) - 1) {
                    cmd_buf[len] = ch;
                    len++;
                }
                break;
            }
        }
    }

    keyboard_wakeup_remove(cpu_get_local()->current);

    return 0;
}

void kmain(void)
{
    struct sys_init *sys;

    kprintf("Seting up task switching\n");
    task_init();

    /* Loop through things to initalize and start them (timer, keyboard, etc...). */
    for (sys = arch_init_systems; sys->name; sys++) {
        kprintf("Starting: %s\n", sys->name);
        (sys->init) ();
    }

    kprintf("Kernel is done booting!\n");

    kernel_is_booting = 0;

    /* scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create());
    scheduler_task_add(task_fake_create()); */

    scheduler_task_add(task_kernel_new_interruptable("Keyboard watch", kernel_keyboard_thread, 0, (const char *[]) { }));

    cpu_start_scheduler();

    panic("ERROR! cpu_start_scheduler() returned!\n");
    while (1)
        hlt();
}

