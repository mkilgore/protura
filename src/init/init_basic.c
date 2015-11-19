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
#include <protura/dump_mem.h>
#include <protura/strtol.h>
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
#include <fs/ext2.h>
#include <fs/file_system.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/stat.h>
#include <fs/namei.h>
#include <fs/vfs.h>
#include <fs/sys.h>
#include <fs/fs.h>
#include <fs/binfmt.h>

#include <init/init_task.h>

static void sleep_for_keyboard(void)
{
  sleep_again:
    sleep {
        if (!arch_keyboard_has_char()) {
            scheduler_task_yield();
            goto sleep_again;
        }
    }
}

static void read_sector(char *buf, sector_t sector)
{
    struct block *b;

    using_block(DEV_MAKE(BLOCK_DEV_IDE, 0), sector, b)
        memcpy(buf, b->data, b->block_size);
}

static void print_file(struct inode *inode, int indent)
{
    int fd;
    struct file *filp;

    kp(KP_DEBUG, "Opening %p\n", inode);

    fd = __sys_open(inode, F(FILE_READABLE), &filp);

    if (fd < 0) {
        kp(KP_DEBUG, "Error opening file: %s\n", strerror(fd));
        return ;
    }

    term_printf("%*s", indent * 2, "");
    term_printf("Size: %d\n", inode->size);

    if (inode->size < 128) {
        char buf[inode->size];
        memset(buf, 0, sizeof(buf));

        vfs_read(filp, buf, sizeof(buf));

        term_printf("%*s", indent * 2, "");
        term_printf("Contents: ");
        int i;
        for (i = 0; i < inode->size; i++)
            term_putchar(buf[i]);
        term_putchar('\n');
    }

    kp(KP_DEBUG, "Closing fd %d\n", fd);

    sys_close(fd);

    kp(KP_DEBUG, "Closed\n");
}

static void print_dir(struct inode *inode, int indent)
{
    __align(4) char dent_buf[sizeof(struct dent) + 128];
    struct dent *dent = (struct dent *)dent_buf;
    struct file *filp;
    int fd;
    int ret;

    fd = __sys_open(inode, F(FILE_READABLE), &filp);

    if (fd < 0) {
        kp(KP_DEBUG, "Error opening file: %s\n", strerror(fd));
        return ;
    }

    kp(KP_TRACE, "Filp: %p\n", filp);

    while ((ret = vfs_read_dent(filp, dent, sizeof(dent_buf))) > 0) {
        struct inode *ent_inode;

        kp(KP_TRACE, "Ret: %d\n", ret);
        term_printf("%*s", indent * 2, "");
        term_printf("Ent: %d, %s\n", dent->ino, dent->name);
        if (strcmp(dent->name, ".") == 0 || strcmp(dent->name, "..") == 0)
            continue;

        using_inode(inode->sb, dent->ino, ent_inode) {
            if (ent_inode && S_ISDIR(ent_inode->mode))
                print_dir(ent_inode, indent + 1);
            else if (ent_inode && S_ISREG(ent_inode->mode))
                print_file(ent_inode, indent + 1);
            else if (ent_inode && S_ISBLK(ent_inode->mode))
                term_printf("BLK device!\n");

            kp(KP_TRACE, "Releaseing inode\n");
        }

        kp(KP_TRACE, "Done using inode\n");
    }

    if (ret < 0) {
        term_printf("Error: %s\n", strerror(ret));
        kp(KP_DEBUG, "Error: %s\n", strerror(ret));
    }

    sys_close(fd);
}

static void test_fs(void)
{
    struct file_system *fs;
    struct super_block *sb;
    struct simple_fs_super_block *simple;
    struct simple_fs_inode *inode;
    struct inode *i;

    fs = file_system_lookup("simple_fs");

    kp(KP_TRACE, "File system: %p\n", fs);

    sb = (fs->read_sb) (DEV_MAKE(BLOCK_DEV_IDE, 0));
    simple  = container_of(sb, struct simple_fs_super_block, sb);

    kp(KP_TRACE, "Simple: %p\n", simple);
    kp(KP_TRACE, "Root: %p\n", simple->sb.root);
    kp(KP_TRACE, "Root ino: %d\n", simple->sb.root->ino);

    inode = container_of(simple->sb.root, struct simple_fs_inode, i);

    kp(KP_TRACE, "Root data sector: %d\n", inode->contents[0]);
    kp(KP_TRACE, "Root size: %d\n", inode->i.size);

    /* print_dir(simple->sb.root, 0); */

    ino_root = inode_dup(simple->sb.root);
    sb_root = sb;

    int ret = namei("/", &i);

    if (ret) {
        kp(KP_TRACE, "namei ret: %d\n", ret);
    } else {
        kp(KP_TRACE, "Inode: %d\n", i->ino);
        kp(KP_TRACE, "Size: %d\n", i->size);
        inode_put(i);
    }

    ret = namei("/test_dir/test_dir2", &i);

    if (ret) {
        kp(KP_TRACE, "namei ret: %d\n", ret);
    } else {
        kp(KP_TRACE, "Inode: %d\n", i->ino);
        kp(KP_TRACE, "Size: %d\n", i->size);
        inode_put(i);
    }

    return ;

}

static void test_ext2(void)
{
    struct file_system *fs;
    struct super_block *sb;
    struct inode *i;

    fs = file_system_lookup("ext2");

    kp(KP_TRACE, "File system: %p\n", fs);

    sb = (fs->read_sb) (DEV_MAKE(BLOCK_DEV_IDE, 0));

    kp(KP_TRACE, "Super-block root: %p\n", sb->root);

    ino_root = inode_dup(sb->root);

    kp(KP_TRACE, "Inode root: %p\n", ino_root);
    kp(KP_TRACE, "Inode root: %d, %d\n", ino_root->ino, ino_root->sb->dev);

    int ret;

    print_dir(ino_root, 0);

    ret = namei("/", &i);

    kp(KP_TRACE, "1: namei ret: %s(%d)\n", strerror(ret), ret);
    if (ret == 0) {
        kp(KP_TRACE, "1: Inode: %d\n", i->ino);
        kp(KP_TRACE, "1: Size: %d\n", i->size);
        kp(KP_TRACE, "1: Mode: %d\n", i->mode);
        kp(KP_TRACE, "1: Dev: %d\n", i->dev);
        kp(KP_TRACE, "1: Nlinks: %d\n", i->nlinks);
        inode_put(i);
    }

    ret = namei("/test_dir/test_dir2", &i);

    kp(KP_TRACE, "2: namei ret: %s(%d)\n", strerror(ret), ret);
    if (ret == 0) {
        kp(KP_TRACE, "2: Inode: %d\n", i->ino);
        kp(KP_TRACE, "2: Size: %d\n", i->size);
        kp(KP_TRACE, "2: Mode: %d\n", i->mode);
        kp(KP_TRACE, "2: Dev: %d\n", i->dev);
        kp(KP_TRACE, "2: Nlinks: %d\n", i->nlinks);
        inode_put(i);
    }

    ret = namei("/test_dir/test_dir2/test_dir2_file", &i);

    kp(KP_TRACE, "3: namei ret: %s(%d)\n", strerror(ret), ret);
    if (ret == 0) {
        kp(KP_TRACE, "3: Inode: %d\n", i->ino);
        kp(KP_TRACE, "3: Size: %d\n", i->size);
        kp(KP_TRACE, "3: Mode: %d\n", i->mode);
        kp(KP_TRACE, "3: Dev: %d\n", i->dev);
        kp(KP_TRACE, "3: Nlinks: %d\n", i->nlinks);
        inode_put(i);
    }

    return ;
}

static char output[80 * 512 / 16 + 1];

int kernel_keyboard_thread(void *unused)
{
    char sector[512];
    char cmd_buf[200];
    int len = 0;
    sector_t s;

    /*
    kp(KP_TRACE, "Keyboard watch task started!\n");
    arch_keyboard_wakeup_add(cpu_get_local()->current); */

    /* test_fs(); */

    test_ext2();

    kp(KP_TRACE, "Creating a user stack for /test_prog...\n");

    struct task *user_task;

    user_task = task_user_new_exec("/test_prog");
    scheduler_task_add(user_task);

    while (1)
        scheduler_task_yield();

    cli();
    while (1)
        hlt()
        ;

    return 0;

    term_printf("Sector: ");

    while (1) {
        int ch;

        sleep_for_keyboard();

        while ((ch = arch_keyboard_get_char()) != -1) {
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
                kp(KP_DEBUG, "%s", output);

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

    arch_keyboard_wakeup_remove(cpu_get_local()->current);

    return 0;
}

