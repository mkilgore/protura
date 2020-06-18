/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/task.h>
#include <protura/mm/kmalloc.h>
#include <protura/fs/vfs.h>
#include <protura/fs/sys.h>
#include <protura/fs/fcntl.h>
#include <protura/fs/fs.h>
#include <protura/fs/namei.h>
#include <protura/fs/inode.h>
#include <protura/ktest.h>

struct vfs_module_priv {
    struct inode *inode;
};

static void set_uidgid(uid_t uid, gid_t gid, uid_t euid, gid_t egid)
{
    struct task *current = cpu_get_local()->current;
    current->creds.uid = uid;
    current->creds.gid = gid;
    current->creds.euid = euid;
    current->creds.egid = egid;
}

static int chown_uid(struct inode *inode, uid_t initial, uid_t target)
{
    inode->uid = initial;

    struct inode_attributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.uid = target;

    return vfs_apply_attributes(inode, F(INODE_ATTR_UID), &attrs);
}

static int chown_gid(struct inode *inode, gid_t initial, gid_t target)
{
    inode->gid = initial;

    struct inode_attributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.gid = target;

    return vfs_apply_attributes(inode, F(INODE_ATTR_GID), &attrs);
}

/* Root (effective UID) can change any UID to any other UID */
static void chown_uid_effective_root(struct ktest *kt)
{
    uid_t initial = KT_ARG(kt, 0, uid_t);
    uid_t target = KT_ARG(kt, 1, uid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(KT_ARG(kt, 2, int), KT_ARG(kt, 2, int), 0, 0);

    int err = chown_uid(priv->inode, initial, target);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, target, priv->inode->uid);
}

/* chown always fails with -EPERM if the user is not root */
static void chown_uid_user_eperm(struct ktest *kt)
{
    uid_t initial = KT_ARG(kt, 0, uid_t);
    uid_t target = KT_ARG(kt, 1, uid_t);
    uid_t user_uid = KT_ARG(kt, 2, uid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, user_uid, user_uid);

    int err = chown_uid(priv->inode, initial, target);

    ktest_assert_equal(kt, -EPERM, err);
    ktest_assert_equal(kt, initial, priv->inode->uid);
}

/* A user is allowed to chown a file to themselves */
static void chown_uid_user_to_themselves_allowed(struct ktest *kt)
{
    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, 1000, 1000);

    int err = chown_uid(priv->inode, 1000, 1000);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, 1000, priv->inode->uid);
}

/* Root (effective UID) can change any GID to any other GID */
static void chown_gid_effective_root(struct ktest *kt)
{
    gid_t initial = KT_ARG(kt, 0, gid_t);
    gid_t target = KT_ARG(kt, 1, gid_t);
    uid_t file_uid = KT_ARG(kt, 2, uid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(KT_ARG(kt, 3, int), KT_ARG(kt, 3, int), 0, 0);

    priv->inode->uid = file_uid;

    int err = chown_gid(priv->inode, initial, target);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, target, priv->inode->gid);
}

static void chown_gid_user_not_owned_eperm(struct ktest *kt)
{
    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, 1000, 1000);

    priv->inode->uid = 500;

    int err = chown_gid(priv->inode, 1000, 1000);

    ktest_assert_equal(kt, -EPERM, err);
    ktest_assert_equal(kt, 500, priv->inode->uid);
    ktest_assert_equal(kt, 1000, priv->inode->gid);
}

static void chown_gid_user_in_group_allowed(struct ktest *kt)
{
    gid_t initial = KT_ARG(kt, 0, gid_t);
    gid_t target = KT_ARG(kt, 1, gid_t);
    uid_t user_uid = KT_ARG(kt, 2, uid_t);
    gid_t user_gid = KT_ARG(kt, 3, gid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, user_uid, user_gid);

    priv->inode->uid = user_uid;

    int err = chown_gid(priv->inode, initial, target);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, user_uid, priv->inode->uid);
    ktest_assert_equal(kt, target, priv->inode->gid);
}

static void chown_clears_suid_sgid(struct ktest *kt)
{
    uid_t user_uid = KT_ARG(kt, 0, uid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, user_uid, user_uid);

    priv->inode->uid = user_uid;
    priv->inode->gid = user_uid;
    priv->inode->mode |= S_ISUID | S_ISGID;

    int err = vfs_chown(priv->inode, -1, -1);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, 0777, priv->inode->mode & 07777);
}

static void chmod_effective_root_allowed(struct ktest *kt)
{
    mode_t initial = KT_ARG(kt, 0, mode_t);
    mode_t target = KT_ARG(kt, 1, mode_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, 0, 0);

    priv->inode->uid = 0;
    priv->inode->gid = 0;

    priv->inode->mode = initial | S_IFREG;

    int err = vfs_chmod(priv->inode, target);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, target, priv->inode->mode & 07777);
}

static void chmod_user_owned_allowed(struct ktest *kt)
{
    mode_t initial = KT_ARG(kt, 0, mode_t);
    mode_t target = KT_ARG(kt, 1, mode_t);
    mode_t uid = KT_ARG(kt, 2, uid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, uid, uid);

    priv->inode->uid = uid;
    priv->inode->gid = uid;

    priv->inode->mode = initial | S_IFREG;

    int err = vfs_chmod(priv->inode, target);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, target, priv->inode->mode & 07777);
}

static void chmod_user_not_owned_eperm(struct ktest *kt)
{
    mode_t initial = KT_ARG(kt, 0, mode_t);
    mode_t target = KT_ARG(kt, 1, mode_t);
    mode_t uid = KT_ARG(kt, 2, uid_t);
    mode_t inode_uid = KT_ARG(kt, 3, uid_t);

    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, uid, uid);

    priv->inode->uid = inode_uid;
    priv->inode->gid = inode_uid;

    priv->inode->mode = initial | S_IFREG;

    int err = vfs_chmod(priv->inode, target);

    ktest_assert_equal(kt, -EPERM, err);
    ktest_assert_equal(kt, initial, priv->inode->mode & 07777);
}

static void chmod_user_not_in_group_clears_sgid(struct ktest *kt)
{
    struct vfs_module_priv *priv = ktest_get_mod_priv(kt);
    set_uidgid(0, 0, 1000, 1000);

    priv->inode->uid = 1000;
    priv->inode->gid = 2000;

    priv->inode->mode = 0555 | S_ISGID | S_IFREG;

    int err = vfs_chmod(priv->inode, 0777 | S_ISGID | S_ISUID);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal(kt, 0777 | S_ISUID, priv->inode->mode & 07777);
}

static int vfs_tests_setup(struct ktest_module *mod)
{
    int err = 0;
    struct vfs_module_priv *priv = kmalloc(sizeof(*priv), PAL_KERNEL);
    if (!priv)
        return -ENOMEM;

    mod->priv = priv;

    int fd = sys_open(make_kernel_buffer("/tmp/vfs-test"), O_RDWR | O_CREAT, 0777);
    if (fd < 0)
        return -EBADF;

    err = sys_close(fd);
    if (err)
        return err;

    struct nameidata data;
    memset(&data, 0, sizeof(data));
    data.path = "/tmp/vfs-test";
    data.cwd = ino_root;

    err = namei_full(&data, F(NAMEI_GET_INODE));

    if (!data.found)
        return -EINVAL;

    priv->inode = data.found;

    int def_groups[3] = { 20, 40, 60 };
    err = sys_setgroups(ARRAY_SIZE(def_groups), make_kernel_buffer(def_groups));
    if (err < 0)
        return err;

    return 0;
}

static int vfs_tests_teardown(struct ktest_module *mod)
{
    struct vfs_module_priv *priv = mod->priv;

    inode_put(priv->inode);

    kfree(priv);

    return 0;
}

static const struct ktest_unit vfs_test_units[] = {
    KTEST_UNIT("chown-uid-effective-root", chown_uid_effective_root,
            (KT_INT(0),    KT_INT(0),    KT_INT(0)),
            (KT_INT(1000), KT_INT(0),    KT_INT(0)),
            (KT_INT(0),    KT_INT(1000), KT_INT(0)),
            (KT_INT(1000), KT_INT(1000), KT_INT(0)),
            (KT_INT(1000), KT_INT(1001), KT_INT(0)),

            (KT_INT(0),    KT_INT(0),    KT_INT(500)),
            (KT_INT(1000), KT_INT(0),    KT_INT(500)),
            (KT_INT(0),    KT_INT(1000), KT_INT(500)),
            (KT_INT(1000), KT_INT(1000), KT_INT(500)),
            (KT_INT(1000), KT_INT(1001), KT_INT(500)),

            (KT_INT(0),    KT_INT(0),    KT_INT(1000)),
            (KT_INT(1000), KT_INT(0),    KT_INT(1000)),
            (KT_INT(0),    KT_INT(1000), KT_INT(1000)),
            (KT_INT(1000), KT_INT(1000), KT_INT(1000)),
            (KT_INT(1000), KT_INT(1001), KT_INT(1000))),

    KTEST_UNIT("chown-gid-effective-root", chown_gid_effective_root,
            (KT_INT(0),    KT_INT(0),    KT_INT(0), KT_INT(0)),
            (KT_INT(1000), KT_INT(0),    KT_INT(0), KT_INT(0)),
            (KT_INT(0),    KT_INT(1000), KT_INT(0), KT_INT(0)),
            (KT_INT(1000), KT_INT(1000), KT_INT(0), KT_INT(0)),
            (KT_INT(1000), KT_INT(1001), KT_INT(0), KT_INT(0)),

            (KT_INT(0),    KT_INT(0),    KT_INT(500), KT_INT(0)),
            (KT_INT(1000), KT_INT(0),    KT_INT(500), KT_INT(0)),
            (KT_INT(0),    KT_INT(1000), KT_INT(500), KT_INT(0)),
            (KT_INT(1000), KT_INT(1000), KT_INT(500), KT_INT(0)),
            (KT_INT(1000), KT_INT(1001), KT_INT(500), KT_INT(0)),

            (KT_INT(0),    KT_INT(0),    KT_INT(0), KT_INT(500)),
            (KT_INT(1000), KT_INT(0),    KT_INT(0), KT_INT(500)),
            (KT_INT(0),    KT_INT(1000), KT_INT(0), KT_INT(500)),
            (KT_INT(1000), KT_INT(1000), KT_INT(0), KT_INT(500)),
            (KT_INT(1000), KT_INT(1001), KT_INT(0), KT_INT(500)),

            (KT_INT(1000), KT_INT(0),    KT_INT(0), KT_INT(1000)),
            (KT_INT(0),    KT_INT(1000), KT_INT(0), KT_INT(1000)),
            (KT_INT(1000), KT_INT(1000), KT_INT(0), KT_INT(1000)),
            (KT_INT(1000), KT_INT(1001), KT_INT(0), KT_INT(1000))),

    KTEST_UNIT("chown-uid-user-eperm", chown_uid_user_eperm,
            (KT_INT(0), KT_INT(0), KT_INT(500)),
            (KT_INT(500), KT_INT(0), KT_INT(500)),
            (KT_INT(0), KT_INT(500), KT_INT(500)),
            (KT_INT(500), KT_INT(200), KT_INT(500)),
            (KT_INT(200), KT_INT(200), KT_INT(500))),

    KTEST_UNIT("chown-uid-user-to-themselves-allowed", chown_uid_user_to_themselves_allowed),
    KTEST_UNIT("chown-gid-user-not-owned-eperm", chown_gid_user_not_owned_eperm),

    KTEST_UNIT("chown-gid-user-in-group-allowed", chown_gid_user_in_group_allowed,
            (KT_INT(20),   KT_INT(20),   KT_INT(1000), KT_INT(1000)),
            (KT_INT(1000), KT_INT(20),   KT_INT(1000), KT_INT(1000)),
            (KT_INT(20),   KT_INT(1000), KT_INT(1000), KT_INT(1000)),
            (KT_INT(20),   KT_INT(40),   KT_INT(1000), KT_INT(1000)),

            (KT_INT(20),   KT_INT(20),   KT_INT(1000), KT_INT(60)),
            (KT_INT(1000), KT_INT(20),   KT_INT(1000), KT_INT(60)),
            (KT_INT(20),   KT_INT(40),   KT_INT(1000), KT_INT(60)),

            (KT_INT(20),   KT_INT(20),   KT_INT(500), KT_INT(1000)),
            (KT_INT(1000), KT_INT(20),   KT_INT(500), KT_INT(1000)),
            (KT_INT(20),   KT_INT(1000), KT_INT(500), KT_INT(1000)),
            (KT_INT(20),   KT_INT(40),   KT_INT(500), KT_INT(1000))),

    KTEST_UNIT("chown-clears-suid-sgid", chown_clears_suid_sgid,
            (KT_INT(0)),
            (KT_INT(1000))),

    KTEST_UNIT("chmod-effective-root-allowed", chmod_effective_root_allowed,
            (KT_UINT(00000), KT_UINT(00000)),
            (KT_UINT(00000), KT_UINT(07777)),
            (KT_UINT(00777), KT_UINT(00000)),
            (KT_UINT(00777), KT_UINT(07777)),
            (KT_UINT(00555), KT_UINT(00000)),
            (KT_UINT(00555), KT_UINT(00666)),
            (KT_UINT(07777), KT_UINT(00777)),
            (KT_UINT(00707), KT_UINT(00000))),

    KTEST_UNIT("chmod-user-owned-allowed", chmod_user_owned_allowed,
            (KT_UINT(00000), KT_UINT(00000), KT_INT(1000)),
            (KT_UINT(00000), KT_UINT(07777), KT_INT(1000)),
            (KT_UINT(00777), KT_UINT(00000), KT_INT(1000)),
            (KT_UINT(00777), KT_UINT(07777), KT_INT(1000)),
            (KT_UINT(00555), KT_UINT(00000), KT_INT(500)),
            (KT_UINT(00555), KT_UINT(00666), KT_INT(500)),
            (KT_UINT(07777), KT_UINT(00777), KT_INT(500)),
            (KT_UINT(00707), KT_UINT(00000), KT_INT(500))),

    KTEST_UNIT("chmod-user-not-owned-eperm", chmod_user_not_owned_eperm,
            (KT_UINT(00000), KT_UINT(00000), KT_INT(1000), KT_INT(0)),
            (KT_UINT(00000), KT_UINT(07777), KT_INT(1000), KT_INT(0)),
            (KT_UINT(00777), KT_UINT(00000), KT_INT(1000), KT_INT(500)),
            (KT_UINT(00777), KT_UINT(07777), KT_INT(1000), KT_INT(500)),
            (KT_UINT(00555), KT_UINT(00000), KT_INT(500),  KT_INT(0)),
            (KT_UINT(00555), KT_UINT(00666), KT_INT(500),  KT_INT(0)),
            (KT_UINT(07777), KT_UINT(00777), KT_INT(500),  KT_INT(1000)),
            (KT_UINT(00707), KT_UINT(00000), KT_INT(500),  KT_INT(1000))),

    KTEST_UNIT("chmod-user-not-in-group-clears-sgid", chmod_user_not_in_group_clears_sgid),
};

KTEST_MODULE_DEFINE("vfs-apply-attributes", vfs_test_units, vfs_tests_setup, vfs_tests_teardown);
