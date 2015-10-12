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
#include <protura/strtol.h>
#include <protura/dump_mem.h>
#include <arch/spinlock.h>
#include <mm/kmalloc.h>
#include <drivers/term.h>
#include <drivers/ide.h>
#include <arch/asm.h>
#include <arch/reset.h>
#include <arch/drivers/keyboard.h>
#include <arch/task.h>
#include <protura/scheduler.h>
#include <arch/cpu.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/backtrace.h>
#include <fs/simple_fs.h>
#include <fs/file_system.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/stat.h>
#include <fs/namei.h>
#include <fs/fs.h>

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

void read_sector(char *buf, sector_t sector)
{
    struct block *b;
    kprintf("Starting IDE test!\n");

    using_block(DEV_MAKE(DEV_IDE, 0), sector, b) {
        memcpy(buf, b->data, b->block_size);

        block_mark_dirty(b);
    }
}

void print_file(struct inode *inode, int indent)
{
    char buf[inode->size];
    int fd;
    struct file *filp;

    memset(buf, 0, sizeof(buf));

    fd = fd_open(inode, &filp);

    if (fd < 0) {
        kprintf("Error opening file: %s\n", strerror(fd));
        return ;
    }

    file_read(filp, buf, sizeof(buf));

    term_printf("%*s", indent * 2, "");
    term_printf("Size: %d\n", inode->size);

    term_printf("%*s", indent * 2, "");
    term_printf("Contents: ");
    int i;
    for (i = 0; i < inode->size; i++)
        term_putchar(buf[i]);
    term_putchar('\n');

    fd_close(fd);
}

void print_dir(struct inode *inode, int indent)
{
    struct dirent ent;
    struct file *filp;
    int fd;

    fd = fd_open(inode, &filp);

    if (fd < 0) {
        kprintf("Error opening file: %s\n", strerror(fd));
        return ;
    }

    while ((file_read(filp, &ent, sizeof(ent))) != 0) {
        struct inode *ent_inode;

        term_printf("%*s", indent * 2, "");
        term_printf("Ent: %d, %s\n", ent.ino, ent.name);
        if (strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0)
            continue;

        using_inode(inode->sb, ent.ino, ent_inode) {
            if (ent_inode && S_ISDIR(ent_inode->mode))
                print_dir(ent_inode, indent + 1);
            else
                print_file(ent_inode, indent + 1);
        }
    }

    fd_close(fd);
}

void test_fs(void)
{
    struct file_system *fs;
    struct super_block *sb;
    struct simple_fs_super_block *simple;
    struct simple_fs_inode *inode;
    struct inode *i;

    fs = file_system_lookup("simple_fs");

    kprintf("File system: %p\n", fs);

    sb = (fs->read_sb) (DEV_MAKE(DEV_IDE, 0));
    simple  = container_of(sb, struct simple_fs_super_block, sb);

    kprintf("Simple: %p\n", simple);
    kprintf("Root: %p\n", simple->sb.root);
    kprintf("Root ino: %d\n", simple->sb.root->ino);

    inode = container_of(simple->sb.root, struct simple_fs_inode, i);

    kprintf("Root data sector: %d\n", inode->contents[0]);
    kprintf("Root size: %d\n", inode->i.size);

    print_dir(simple->sb.root, 0);

    ino_root = inode_dup(simple->sb.root);

    int ret = namei("/", &i);

    if (ret) {
        kprintf("namei ret: %d\n", ret);
    } else {
        kprintf("Inode: %d\n", i->ino);
        kprintf("Size: %d\n", i->size);
        inode_put(i);
    }

    ret = namei("/test_dir/test_dir2", &i);

    if (ret) {
        kprintf("namei ret: %d\n", ret);
    } else {
        kprintf("Inode: %d\n", i->ino);
        kprintf("Size: %d\n", i->size);
        inode_put(i);
    }

    sb_put(sb);
}

static char output[80 * 512 / 16 + 1];

int kernel_keyboard_thread(void *unused)
{
    char sector[512];
    char cmd_buf[200];
    int len = 0;
    sector_t s;

    kprintf("Keyboard watch task started!\n");
    keyboard_wakeup_add(cpu_get_local()->current);

    test_fs();

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

/* 
 * We want to get to a process context as soon as possible, as not being in one
 * complicates what we can and can't do (For example, cpu_get_local()->current
 * is NULL until we enter a process context, so we can't sleep and we can't
 * register for wait queues, take mutex's, etc...). This is similar to an
 * interrupt context.
 *
 * To achieve this, kmain almost immedieatly calls task_init_start to create a
 * task, and then starts the scheduler. The scheduler will then start our new
 * task to finish the rest of the boot in a new process context.
 */

int init_second_half(void *unused)
{
    struct sys_init *sys;

    cpu_setup_idle();

    /* Loop through things to initalize and start them (timer, keyboard, etc...). */
    for (sys = arch_init_systems; sys->name; sys++) {
        kprintf("Starting: %s\n", sys->name);
        (sys->init) ();
    }

    kprintf("Kernel is done booting!\n");
    kernel_is_booting = 0;

    scheduler_task_add(task_kernel_new_interruptable("Keyboard watch", kernel_keyboard_thread, NULL));

    return 0;
}

void kmain(void)
{
    kprintf("Seting up task switching\n");
    task_init_start(init_second_half, NULL);

    kprintf("Starting scheduler\n");
    cpu_start_scheduler();

    panic("ERROR! cpu_start_scheduler() returned!\n");
    while (1)
        hlt();
}

