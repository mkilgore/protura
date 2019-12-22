/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests syscall parameters to verify that invalid user pointers don't trigger
 * panic's and such.
 *
 * NOTE: !!! These tests require a basic root filesystem to be mounted !!!
 *
 * Also note, these tests look a bit more exaustive then they actually are.
 * Most of these calls test pretty much every error condition, but some of them
 * like `read()` and `write()` pass the user buffer to the underlying driver,
 * meaning other drivers/backends are not tested in this mannor. That said,
 * most syscall parameters do not fall into this category, it is basically only
 * syscalls that return data via a buffer.
 *
 * Current untested syscalls: execve, wait, waitpid, recv, recvfrom
 */

#include <protura/types.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/task.h>
#include <protura/fs/sys.h>
#include <protura/fs/fcntl.h>
#include <protura/fs/file.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/fs/sys.h>
#include <protura/net/sys.h>
#include <protura/utsname.h>
#include <protura/ktest.h>

static void syscall_open_test(struct ktest *kt)
{
    int ret = sys_open(KT_ARG(kt, 0, struct user_buffer), 0, 0);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_read_test(struct ktest *kt)
{
    int ret = sys_read(1, KT_ARG(kt, 0, struct user_buffer), 50);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_write_test(struct ktest *kt)
{
    int ret = sys_write(1, KT_ARG(kt, 0, struct user_buffer), 20);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_read_dent_test(struct ktest *kt)
{
    int ret = sys_read_dent(0, KT_ARG(kt, 0, struct user_buffer), 100);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_truncate_test(struct ktest *kt)
{
    int ret = sys_truncate(KT_ARG(kt, 0, struct user_buffer), 1000);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_link_test(struct ktest *kt)
{
    int ret = sys_link(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_unlink_test(struct ktest *kt)
{
    int ret = sys_unlink(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_chdir_test(struct ktest *kt)
{
    int ret = sys_chdir(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_stat_test(struct ktest *kt)
{
    int ret = sys_stat(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_fstat_test(struct ktest *kt)
{
    int ret = sys_fstat(0, KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_mkdir_test(struct ktest *kt)
{
    int ret = sys_mkdir(KT_ARG(kt, 0, struct user_buffer), 0);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_mknod_test(struct ktest *kt)
{
    int ret = sys_mknod(KT_ARG(kt, 0, struct user_buffer), 0, 0);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_rmdir_test(struct ktest *kt)
{
    int ret = sys_rmdir(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_rename_test(struct ktest *kt)
{
    int ret = sys_rename(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_lstat_test(struct ktest *kt)
{
    int ret = sys_lstat(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_readlink_test(struct ktest *kt)
{
    char krnl_buf[20];
    int ret = sys_readlink(KT_ARG(kt, 0, struct user_buffer), make_kernel_buffer(krnl_buf), sizeof(krnl_buf));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_symlink_test(struct ktest *kt)
{
    int ret = sys_symlink(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_mount_test(struct ktest *kt)
{
    int ret = sys_mount(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer), KT_ARG(kt, 2, struct user_buffer), 0, make_user_buffer(NULL));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_umount_test(struct ktest *kt)
{
    int ret = sys_umount(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_pipe_test(struct ktest *kt)
{
    int ret = sys_pipe(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_sigprocmask_test(struct ktest *kt)
{
    int ret = sys_sigprocmask(0, KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_sigpending_test(struct ktest *kt)
{
    int ret = sys_sigpending(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_sigaction_test(struct ktest *kt)
{
    int ret = sys_sigaction(2, KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_sigwait_test(struct ktest *kt)
{
    int ret = sys_sigwait(KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_sigsuspend_test(struct ktest *kt)
{
    int ret = sys_sigsuspend(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_time_test(struct ktest *kt)
{
    int ret = sys_time(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_getpgrp_test(struct ktest *kt)
{
    int ret = sys_getpgrp(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_poll_test(struct ktest *kt)
{
    int ret = sys_poll(KT_ARG(kt, 0, struct user_buffer), 2, 1);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_sendto_test(struct ktest *kt)
{
    int ret = sys_sendto(2, KT_ARG(kt, 0, struct user_buffer), 20, 0, KT_ARG(kt, 1, struct user_buffer), 10);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_bind_test(struct ktest *kt)
{
    int ret = sys_bind(2, KT_ARG(kt, 0, struct user_buffer), 5);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_getsockname_test(struct ktest *kt)
{
    int ret = sys_getsockname(2, KT_ARG(kt, 0, struct user_buffer), KT_ARG(kt, 1, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_send_test(struct ktest *kt)
{
    int ret = sys_send(2, KT_ARG(kt, 0, struct user_buffer), 20, 0);
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_gettimeofday_test(struct ktest *kt)
{
    int ret = sys_gettimeofday(KT_ARG(kt, 0, struct user_buffer), make_user_buffer(NULL));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_setgroups_test(struct ktest *kt)
{
    int ret = sys_setgroups(2, KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_getgroups_test(struct ktest *kt)
{
    int ret = sys_getgroups(5, KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_uname_test(struct ktest *kt)
{
    int ret = sys_uname(KT_ARG(kt, 0, struct user_buffer));
    ktest_assert_equal(kt, -EFAULT, ret);
}

static void syscall_connect_test(struct ktest *kt)
{
    int ret = sys_connect(2, KT_ARG(kt, 0, struct user_buffer), 10);
    ktest_assert_equal(kt, -EFAULT, ret);
}

struct syscall_module_priv {
    int fd0, fd1, fd2;
    struct page *top_page;
};

static int syscall_tests_setup(struct ktest_module *mod)
{
    struct syscall_module_priv *priv = kmalloc(sizeof(*priv), PAL_KERNEL);
    if (!priv)
        return -ENOMEM;

    mod->priv = priv;

    /* Some of our tests require valid file descriptors, thus we open a few
     * here, one for a directory, and one for a file */
    priv->fd0 = sys_open(make_kernel_buffer("/"), O_RDONLY, 0777);
    if (priv->fd0 != 0)
        return -EBADF;

    priv->fd1 = sys_open(make_kernel_buffer("/tmp/kernel-test"), O_RDWR | O_CREAT, 0777);
    if (priv->fd1 != 1)
        return -EBADF;

    priv->fd2 = sys_socket(AF_INET, SOCK_DGRAM, 0);
    if (priv->fd2 != 2)
        return -EBADF;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    int err = sys_bind(priv->fd2, make_kernel_buffer(&addr), sizeof(addr));
    if (err < 0)
        return err;

    int def_groups[3] = { 20, 40, 60 };
    err = sys_setgroups(ARRAY_SIZE(def_groups), make_kernel_buffer(def_groups));
    if (err < 0)
        return err;

    /* Write some data to make the file have a non-zero length. And then seek
     * to the beginning to prepare for the tests. */
    err = sys_write(priv->fd1, make_kernel_buffer("AAAABBBBCCCCDDDDEEEEFFFF"), 24);
    if (err < 0)
        return err;

    err = sys_lseek(priv->fd1, 0, SEEK_SET);
    if (err < 0)
        return err;

    /* Map a page at the very top of userspace. This allows us to make valid
     * buffers that span into kernelspace */
    priv->top_page = palloc(0, PAL_KERNEL);
    if (!priv->top_page)
        return -ENOMEM;

    /* In some cases, we require strings that extend passed this page. We write
     * 1's to the entire page to ensure there are no NUL characters that would
     * end the string prematurely */
    memset(priv->top_page->virt, 0x01, PG_SIZE);

    struct task *current = cpu_get_local()->current;
    page_table_map_entry(current->addrspc->page_dir, (va_t)0xBFFFFF000, page_to_pa(priv->top_page), VM_MAP_READ | VM_MAP_WRITE);

    return 0;
}

static int syscall_tests_teardown(struct ktest_module *mod)
{
    struct syscall_module_priv *priv = mod->priv;

    struct task *current = cpu_get_local()->current;
    page_table_unmap_entry(current->addrspc->page_dir, (va_t)0xBFFFFF000);

    pfree(priv->top_page, 0);

    sys_close(priv->fd2);
    sys_close(priv->fd1);
    sys_close(priv->fd0);

    kfree(priv);
    return 0;
}

static const struct ktest_unit syscall_test_units[] = {
    KTEST_UNIT("syscall-open-test", syscall_open_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-open-test", syscall_open_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-open-test", syscall_open_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-read-test", syscall_read_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-read-test", syscall_read_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-read-test", syscall_read_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-write-test", syscall_write_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-write-test", syscall_write_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-write-test", syscall_write_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-read-dent-test", syscall_read_dent_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-read-dent-test", syscall_read_dent_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-read-dent-test", syscall_read_dent_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-truncate-test", syscall_truncate_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-truncate-test", syscall_truncate_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-truncate-test", syscall_truncate_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-link-test", syscall_link_test, KT_KERNEL_BUF(""), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-link-test", syscall_link_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-link-test", syscall_link_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-link-test", syscall_link_test, KT_USER_BUF(NULL), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-link-test", syscall_link_test, KT_USER_BUF(0xBFFFFFF0), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-link-test", syscall_link_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF("")),

    KTEST_UNIT("syscall-unlink-test", syscall_unlink_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-unlink-test", syscall_unlink_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-unlink-test", syscall_unlink_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-stat-test", syscall_stat_test, KT_KERNEL_BUF(""), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-stat-test", syscall_stat_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-stat-test", syscall_stat_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-stat-test", syscall_stat_test, KT_USER_BUF(NULL), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-stat-test", syscall_stat_test, KT_USER_BUF(0xBFFFFFF0), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-stat-test", syscall_stat_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF("")),

    KTEST_UNIT("syscall-fstat-test", syscall_fstat_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-fstat-test", syscall_fstat_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-fstat-test", syscall_fstat_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-mkdir-test", syscall_mkdir_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-mkdir-test", syscall_mkdir_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-mkdir-test", syscall_mkdir_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-mknod-test", syscall_mknod_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-mknod-test", syscall_mknod_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-mknod-test", syscall_mknod_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-rmdir-test", syscall_rmdir_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-rmdir-test", syscall_rmdir_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-rmdir-test", syscall_rmdir_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-rename-test", syscall_rename_test, KT_KERNEL_BUF(""), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-rename-test", syscall_rename_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-rename-test", syscall_rename_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-rename-test", syscall_rename_test, KT_USER_BUF(NULL), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-rename-test", syscall_rename_test, KT_USER_BUF(0xBFFFFFF0), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-rename-test", syscall_rename_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF("")),

    KTEST_UNIT("syscall-lstat-test", syscall_lstat_test, KT_KERNEL_BUF(""), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-lstat-test", syscall_lstat_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-lstat-test", syscall_lstat_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-lstat-test", syscall_lstat_test, KT_USER_BUF(NULL), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-lstat-test", syscall_lstat_test, KT_USER_BUF(0xBFFFFFF0), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-lstat-test", syscall_lstat_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF("")),

    KTEST_UNIT("syscall-readlink-test", syscall_readlink_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-readlink-test", syscall_readlink_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-readlink-test", syscall_readlink_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-symlink-test", syscall_symlink_test, KT_KERNEL_BUF(""), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-symlink-test", syscall_symlink_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-symlink-test", syscall_symlink_test, KT_KERNEL_BUF(""), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-symlink-test", syscall_symlink_test, KT_USER_BUF(NULL), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-symlink-test", syscall_symlink_test, KT_USER_BUF(0xBFFFFFF0), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-symlink-test", syscall_symlink_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF("")),

    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_KERNEL_BUF("/dev/hda1"), KT_USER_BUF(NULL),       KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_KERNEL_BUF("/dev/hda1"), KT_USER_BUF(0xBFFFFFF0), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_KERNEL_BUF("/dev/hda1"), KT_USER_BUF(0xC0200000), KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_USER_BUF(0xBFFFFFF0),    KT_KERNEL_BUF(""),       KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_USER_BUF(0xC0200000),    KT_KERNEL_BUF(""),       KT_KERNEL_BUF("")),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_KERNEL_BUF("/dev/hda1"), KT_KERNEL_BUF(""),       KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_KERNEL_BUF("/dev/hda1"), KT_KERNEL_BUF(""),       KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_KERNEL_BUF("/dev/hda1"), KT_KERNEL_BUF(""),       KT_USER_BUF(0xC0200000)),

    /* Passing NULL as the source to mount() is actually valid */
    /* KTEST_UNIT("syscall-mount-test", syscall_mount_test, KT_USER_BUF(NULL),          KT_KERNEL_BUF(""),       KT_KERNEL_BUF("")), */

    KTEST_UNIT("syscall-umount-test", syscall_umount_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-umount-test", syscall_umount_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-umount-test", syscall_umount_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-chdir-test", syscall_chdir_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-chdir-test", syscall_chdir_test, KT_USER_BUF(0xBFFFFFF0)),
    KTEST_UNIT("syscall-chdir-test", syscall_chdir_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-pipe-test", syscall_pipe_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-pipe-test", syscall_pipe_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-pipe-test", syscall_pipe_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-sigprocmask-test", syscall_sigprocmask_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-sigprocmask-test", syscall_sigprocmask_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xBFFFFFFF)),
    KTEST_UNIT("syscall-sigprocmask-test", syscall_sigprocmask_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-sigprocmask-test", syscall_sigprocmask_test, KT_USER_BUF(NULL),       KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sigprocmask-test", syscall_sigprocmask_test, KT_USER_BUF(0xBFFFFFFF), KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sigprocmask-test", syscall_sigprocmask_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF(0xBFFFFF00)),

    KTEST_UNIT("syscall-sigpending-test", syscall_sigpending_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-sigpending-test", syscall_sigpending_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-sigpending-test", syscall_sigpending_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-sigaction-test", syscall_sigaction_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-sigaction-test", syscall_sigaction_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xBFFFFFFF)),
    KTEST_UNIT("syscall-sigaction-test", syscall_sigaction_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-sigaction-test", syscall_sigaction_test, KT_USER_BUF(NULL),       KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sigaction-test", syscall_sigaction_test, KT_USER_BUF(0xBFFFFFFF), KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sigaction-test", syscall_sigaction_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF(0xBFFFFF00)),

    KTEST_UNIT("syscall-sigwait-test", syscall_sigwait_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-sigwait-test", syscall_sigwait_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xBFFFFFFF)),
    KTEST_UNIT("syscall-sigwait-test", syscall_sigwait_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-sigwait-test", syscall_sigwait_test, KT_USER_BUF(NULL),       KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sigwait-test", syscall_sigwait_test, KT_USER_BUF(0xBFFFFFFF), KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sigwait-test", syscall_sigwait_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF(0xBFFFFF00)),

    KTEST_UNIT("syscall-sigsuspend-test", syscall_sigsuspend_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-sigsuspend-test", syscall_sigsuspend_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-sigsuspend-test", syscall_sigsuspend_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-time-test", syscall_time_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-time-test", syscall_time_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-time-test", syscall_time_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-getpgrp-test", syscall_getpgrp_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-getpgrp-test", syscall_getpgrp_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-getpgrp-test", syscall_getpgrp_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-poll-test", syscall_poll_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-poll-test", syscall_poll_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-poll-test", syscall_poll_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-sendto-test", syscall_sendto_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-sendto-test", syscall_sendto_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xBFFFFFFF)),
    KTEST_UNIT("syscall-sendto-test", syscall_sendto_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-sendto-test", syscall_sendto_test, KT_USER_BUF(NULL),       KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sendto-test", syscall_sendto_test, KT_USER_BUF(0xBFFFFFFF), KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-sendto-test", syscall_sendto_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF(0xBFFFFF00)),

    KTEST_UNIT("syscall-bind-test", syscall_bind_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-bind-test", syscall_bind_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-bind-test", syscall_bind_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-getsockname-test", syscall_getsockname_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-getsockname-test", syscall_getsockname_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xBFFFFFFF)),
    KTEST_UNIT("syscall-getsockname-test", syscall_getsockname_test, KT_KERNEL_BUF(0xBFFFFF00), KT_USER_BUF(0xC0200000)),
    KTEST_UNIT("syscall-getsockname-test", syscall_getsockname_test, KT_USER_BUF(NULL),       KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-getsockname-test", syscall_getsockname_test, KT_USER_BUF(0xBFFFFFFF), KT_KERNEL_BUF(0xBFFFFF00)),
    KTEST_UNIT("syscall-getsockname-test", syscall_getsockname_test, KT_USER_BUF(0xC0200000), KT_KERNEL_BUF(0xBFFFFF00)),

    KTEST_UNIT("syscall-send-test", syscall_send_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-send-test", syscall_send_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-send-test", syscall_send_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-gettimeofday-test", syscall_gettimeofday_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-gettimeofday-test", syscall_gettimeofday_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-gettimeofday-test", syscall_gettimeofday_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-setgroups-test", syscall_setgroups_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-setgroups-test", syscall_setgroups_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-setgroups-test", syscall_setgroups_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-getgroups-test", syscall_getgroups_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-getgroups-test", syscall_getgroups_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-getgroups-test", syscall_getgroups_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-uname-test", syscall_uname_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-uname-test", syscall_uname_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-uname-test", syscall_uname_test, KT_USER_BUF(0xC0200000)),

    KTEST_UNIT("syscall-connect-test", syscall_connect_test, KT_USER_BUF(NULL)),
    KTEST_UNIT("syscall-connect-test", syscall_connect_test, KT_USER_BUF(0xBFFFFFFE)),
    KTEST_UNIT("syscall-connect-test", syscall_connect_test, KT_USER_BUF(0xC0200000)),
};

KTEST_MODULE_DEFINE("syscall", syscall_test_units, syscall_tests_setup, syscall_tests_teardown);
