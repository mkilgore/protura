/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for inode_table.c - included directly at the end of inode_table.c
 */

#include <protura/types.h>
#include <protura/fs/inode.h>
#include <protura/mm/slab.h>
#include <protura/ktest.h>

/* This is the general delay we introduce when queueing actions to take place
 * on the inode table, for testing various wait states */
#define INODE_DELAY 50

enum {
    INODE_ALLOC_CALLED,
    INODE_DEALLOC_CALLED,
    INODE_WRITE_CALLED,
    INODE_DELETE_CALLED,
    INODE_READ_CALLED,
    SB_WRITE_CALLED,
    SB_PUT_CALLED,
};

struct super_block_fake {
    struct super_block sb;

    flags_t called;

    unsigned int inode_lock_was_set :1;
    flags_t inode_flags;

    struct inode *inode;
    struct delay_work work;
};

struct module_priv {
    struct super_block_fake *fake;
    ino_t next_ino;
    void *temp_ptr_pages; /* 32 pages of temporary memory for holding tables */
};

static struct inode *fake_inode_alloc(struct super_block *sb)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, INODE_ALLOC_CALLED);

    struct inode *i = kzalloc(sizeof(struct inode), PAL_KERNEL);
    inode_init(i);
    return i;
}

static int fake_inode_dealloc(struct super_block *sb, struct inode *inode)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, INODE_DEALLOC_CALLED);

    kfree(inode);
    return 0;
}

static int fake_inode_write(struct super_block *sb, struct inode *inode)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);

    /* Allows testing inode_wait_for_write() waits for INO_SYNC to be cleared */
    scheduler_task_waitms(INODE_DELAY);
    fake->inode_lock_was_set = mutex_is_locked(&fake->inode->lock);
    fake->inode_flags = fake->inode->flags;

    flag_set(&fake->called, INODE_WRITE_CALLED);
    return 0;
}

static int fake_inode_write_nowait(struct super_block *sb, struct inode *inode)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, INODE_WRITE_CALLED);
    return 0;
}

static int fake_inode_read(struct super_block *sb, struct inode *inode)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, INODE_READ_CALLED);
    return 0;
}

static int fake_inode_delete(struct super_block *sb, struct inode *inode)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);

    /* Allows testing inode_wait_for_write() waits for INO_FREEING to be cleared */
    scheduler_task_waitms(INODE_DELAY);
    fake->inode_lock_was_set = mutex_is_locked(&fake->inode->lock);
    fake->inode_flags = fake->inode->flags;

    flag_set(&fake->called, INODE_DELETE_CALLED);
    return 0;
}

static int fake_inode_delete_nowait(struct super_block *sb, struct inode *inode)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, INODE_DELETE_CALLED);
    return 0;
}

static int fake_sb_write(struct super_block *sb)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, SB_WRITE_CALLED);
    return 0;
}

static int fake_sb_put(struct super_block *sb)
{
    struct super_block_fake *fake = container_of(sb, struct super_block_fake, sb);
    flag_set(&fake->called, SB_PUT_CALLED);
    return 0;
}

static struct super_block_ops fake_ops = {
    .inode_dealloc = fake_inode_dealloc,
    .inode_alloc = fake_inode_alloc,
    .inode_write = fake_inode_write,
    .inode_delete = fake_inode_delete,
    .inode_read = fake_inode_read,
    .sb_write = fake_sb_write,
    .sb_put = fake_sb_put,
};

static struct super_block_ops fake_ops_nowait = {
    .inode_dealloc = fake_inode_dealloc,
    .inode_alloc = fake_inode_alloc,
    .inode_write = fake_inode_write_nowait,
    .inode_delete = fake_inode_delete_nowait,
    .inode_read = fake_inode_read,
    .sb_write = fake_sb_write,
    .sb_put = fake_sb_put,
};

static void mark_inode_bad_callback(struct work *w)
{
    struct super_block_fake *fake = container_of(w, struct super_block_fake, work.work);
    inode_mark_bad(fake->inode);
}

static void mark_inode_valid_callback(struct work *w)
{
    struct super_block_fake *fake = container_of(w, struct super_block_fake, work.work);
    inode_mark_valid(fake->inode);
}

static void write_inode_callback(struct work *w)
{
    struct super_block_fake *fake = container_of(w, struct super_block_fake, work.work);
    inode_write_to_disk(fake->inode, 1);
}

static void drop_inode_callback(struct work *w)
{
    struct super_block_fake *fake = container_of(w, struct super_block_fake, work.work);
    inode_put(fake->inode);
}

/* Verify that inode_get() waits for the inode from inode_get_invalid to be
 * marked valid/bad, and returns the inode when it is marked valid */
static void test_mark_inode_valid_queue(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get_invalid(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, 0, fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    kwork_delay_schedule_callback(&fake->work, INODE_DELAY, mark_inode_valid_callback);

    struct inode *result = inode_get(&fake->sb, ino);
    ktest_assert_notequal(kt, NULL, result);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED), fake->called);
    ktest_assert_equal(kt, 2, atomic_get(&fake->inode->ref));

    inode_put(result);
    inode_put(fake->inode);
}

/* Verify that inode_get() waits for the inode from inode_get_invalid to be
 * marked valid/bad, and returns NULL when it is marked bad */
static void test_mark_inode_bad_queue(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get_invalid(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, 0, fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    kwork_delay_schedule_callback(&fake->work, INODE_DELAY, mark_inode_bad_callback);

    struct inode *result = inode_get(&fake->sb, ino);
    ktest_assert_equal(kt, NULL, result);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_DEALLOC_CALLED), fake->called);
}

/* Verify that inode_get() calls inode_read if the inode does not exist */
static void test_inode_get_calls_inode_read(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    inode_put(fake->inode);
}

static void test_multiple_inode_get_return_same_inode(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    fake->called = 0;

    struct inode *same = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, same);
    ktest_assert_equal(kt, fake->inode, same);
    ktest_assert_equal(kt, 0, fake->called);
    ktest_assert_equal(kt, 2, atomic_get(&same->ref));

    inode_put(fake->inode);
    inode_put(fake->inode);
}

static void test_inode_dirty_write_to_disk_calls_inode_write(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    flag_set(&fake->inode->flags, INO_DIRTY);

    int ret = inode_write_to_disk(fake->inode, 1);

    ktest_assert_equal(kt, 0, ret);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED, INODE_WRITE_CALLED), fake->called);
    ktest_assert_equal(kt, 1, fake->inode_lock_was_set);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    inode_put(fake->inode);
}

static void test_inode_not_dirty_write_to_disk_does_nothing(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    int ret = inode_write_to_disk(fake->inode, 1);

    /* inode_write is not called */
    ktest_assert_equal(kt, 0, ret);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    inode_put(fake->inode);
}

static void test_inode_wait_for_write(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    flag_set(&fake->inode->flags, INO_DIRTY);

    kwork_schedule_callback(&fake->work.work, write_inode_callback);

    /* Wait for write_inode_callback to start syncing the inode. Should happen pretty fast, generally. */
    scheduler_task_waitms(10);

    ktest_assert_equal(kt, F(INO_VALID, INO_DIRTY, INO_SYNC), fake->inode->flags);

    inode_wait_for_write(fake->inode);

    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED, INODE_WRITE_CALLED), fake->called);
    ktest_assert_equal(kt, F(INO_VALID, INO_DIRTY, INO_SYNC), fake->inode_flags);
    ktest_assert_equal(kt, 1, fake->inode_lock_was_set);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    inode_put(fake->inode);
}

static void test_inode_dirty_put_adds_to_list(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));
    fake->called = 0;

    atomic_set(&fake->inode->nlinks, 1);
    flag_set(&fake->inode->flags, INO_DIRTY);

    struct inode *dup = inode_dup(fake->inode);
    inode_put(fake->inode);

    ktest_assert_equal(kt, 1, list_node_is_in_list(&dup->sb_dirty_entry));
    ktest_assert_equal(kt, 1, atomic_get(&dup->ref));

    atomic_set(&fake->inode->nlinks, 0);
    inode_put(dup);
}

static void test_inode_wait_on_freeing(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    fake->called = 0;

    kwork_schedule_callback(&fake->work.work, drop_inode_callback);

    /* Wait for drop_inode_callback to start dropping the inode. Should happen pretty fast, generally. */
    scheduler_task_waitms(10);

    ktest_assert_equal(kt, F(INO_VALID, INO_FREEING), fake->inode->flags);

    struct inode *new = inode_get(&fake->sb, ino);

    ktest_assert_equal(kt, F(INO_VALID), new->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED, INODE_DELETE_CALLED, INODE_DEALLOC_CALLED), fake->called);
    ktest_assert_equal(kt, F(INO_VALID, INO_FREEING), fake->inode_flags);
    ktest_assert_equal(kt, 0, fake->inode_lock_was_set);
    ktest_assert_equal(kt, 1, atomic_get(&new->ref));

    inode_put(new);
}

static void test_inode_zero_refs_deleted(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    struct super_block_fake *fake = priv->fake;
    ino_t ino = priv->next_ino++;

    fake->inode = inode_get(&fake->sb, ino);

    ktest_assert_notequal(kt, NULL, fake->inode);
    ktest_assert_equal(kt, F(INO_VALID), fake->inode->flags);
    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);
    ktest_assert_equal(kt, 1, atomic_get(&fake->inode->ref));

    /* IF nlink=0, then delete is called */
    atomic_set(&fake->inode->nlinks, 0);

    inode_put(fake->inode);

    ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED, INODE_DELETE_CALLED, INODE_DEALLOC_CALLED), fake->called);
    ktest_assert_equal(kt, F(INO_VALID, INO_FREEING), fake->inode_flags);
    ktest_assert_equal(kt, 0, fake->inode_lock_was_set);
}

static void test_inode_sync_diff_super(struct ktest *kt)
{
    int inode_count = KT_ARG(kt, 0, int);
    struct module_priv *priv = ktest_get_mod_priv(kt);

    struct super_block_fake *fake = kzalloc(sizeof(*fake), PAL_KERNEL);
    super_block_init(&fake->sb);
    delay_work_init(&fake->work);
    fake->sb.ops = &fake_ops_nowait;

    ino_t next_ino = 20;
    struct inode *root = inode_get(&fake->sb, 2);

    struct inode **new = priv->temp_ptr_pages;
    int i;

    for (i = 0; i < inode_count; i++) {
        new[i] = inode_get(&fake->sb, next_ino++);

        ktest_assert_notequal(kt, NULL, new[i]);
        ktest_assert_equal(kt, 1, atomic_get(&new[i]->ref));
        ktest_assert_equal(kt, F(INO_VALID), new[i]->flags);
        ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);

        fake->called = 0;
        flag_set(&new[i]->flags, INO_DIRTY);
        atomic_set(&new[i]->nlinks, 1);

        /* Acquire and drop a reference to put inode on dirty list */
        inode_put(inode_dup(new[i]));
        ktest_assert_equal(kt, 1, list_node_is_in_list(&new[i]->sb_dirty_entry));
    }

    /* NOTE: We're syncing the *wrong* super-block here, so it shouldn't touch
     * any of the new[] inodes */
    inode_sync(&priv->fake->sb, 1);

    ktest_assert_equal(kt, 0, fake->called);
    for (i = 0; i < inode_count; i++) {
        ktest_assert_equal(kt, F(INO_VALID, INO_DIRTY), new[i]->flags);
        inode_put(new[i]);
    }

    int ret = inode_clear_super(&fake->sb, root);
    ktest_assert_equal(kt, 0, ret);
    kfree(fake);
}

static void test_inode_sync(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    int inode_count = KT_ARG(kt, 0, int);
    struct super_block_fake *fake = kzalloc(sizeof(*fake), PAL_KERNEL);
    super_block_init(&fake->sb);
    delay_work_init(&fake->work);
    fake->sb.ops = &fake_ops_nowait;
    ino_t next_ino = 20;

    struct inode **new = priv->temp_ptr_pages;
    struct inode *root = inode_get(&fake->sb, 2);
    int i;

    for (i = 0; i < inode_count; i++) {
        new[i] = inode_get(&fake->sb, next_ino++);

        ktest_assert_notequal(kt, NULL, new[i]);
        ktest_assert_equal(kt, 1, atomic_get(&new[i]->ref));
        ktest_assert_equal(kt, F(INO_VALID), new[i]->flags);
        ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fake->called);

        fake->called = 0;
        flag_set(&new[i]->flags, INO_DIRTY);
        atomic_set(&new[i]->nlinks, 1);

        /* Acquire and drop a reference to put inode on dirty list */
        inode_put(inode_dup(new[i]));
        ktest_assert_equal(kt, 1, list_node_is_in_list(&new[i]->sb_dirty_entry));
    }

    inode_sync(&fake->sb, 1);

    ktest_assert_equal(kt, F(INODE_WRITE_CALLED), fake->called);
    for (i = 0; i < inode_count; i++) {
        ktest_assert_equal(kt, F(INO_VALID), new[i]->flags);
        inode_put(new[i]);
    }

    int ret = inode_clear_super(&fake->sb, root);
    ktest_assert_equal(kt, 0, ret);
    kfree(fake);
}

static void test_inode_sync_all(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    int inode_count = KT_ARG(kt, 0, int);
    int super_count = KT_ARG(kt, 1, int);
    ino_t next_ino = 20;

    struct super_block_fake **fakes = kzalloc(sizeof(*fakes) * super_count, PAL_KERNEL);
    struct inode *(*new)[inode_count] = priv->temp_ptr_pages;
    struct inode **root = kzalloc(sizeof(*root) * super_count, PAL_KERNEL);
    int i, j;

    for (i = 0; i < super_count; i++) {
        fakes[i] = kzalloc(sizeof(*fakes[i]), PAL_KERNEL);
        super_block_init(&fakes[i]->sb);
        delay_work_init(&fakes[i]->work);
        fakes[i]->sb.ops = &fake_ops_nowait;

        root[i] = inode_get(&fakes[i]->sb, 2);
    }

    for (i = 0; i < inode_count; i++, next_ino++) {
        for (j = 0; j < super_count; j++) {
            struct inode *inode = inode_get(&fakes[j]->sb, next_ino);
            new[j][i] = inode;

            ktest_assert_notequal(kt, NULL, inode);
            ktest_assert_equal(kt, 1, atomic_get(&inode->ref));
            ktest_assert_equal(kt, F(INO_VALID), inode->flags);
            ktest_assert_equal(kt, F(INODE_ALLOC_CALLED, INODE_READ_CALLED), fakes[j]->called);

            fakes[j]->called = 0;
            flag_set(&inode->flags, INO_DIRTY);
            atomic_set(&inode->nlinks, 1);

            /* Acquire and drop a reference to put inode on dirty list */
            inode_put(inode_dup(inode));
            ktest_assert_equal(kt, 1, list_node_is_in_list(&inode->sb_dirty_entry));
        }
    }

    inode_sync_all(1);

    for (i = 0; i < super_count; i++) {
        for (j = 0; j < inode_count; j++) {
            struct inode *inode = new[i][j];

            ktest_assert_equal(kt, 1, atomic_get(&inode->ref));
            ktest_assert_equal(kt, F(INO_VALID), inode->flags);
            inode_put(inode);
        }

        inode_clear_super(&fakes[i]->sb, root[i]);
        kfree(fakes[i]);
    }

    kfree(fakes);
    kfree(root);
}

static int inode_table_module_setup(struct ktest_module *mod)
{
    struct module_priv *priv = kzalloc(sizeof(*priv), PAL_KERNEL);
    if (!priv)
        return -ENOMEM;

    mod->priv = priv;

    priv->fake = kzalloc(sizeof(*priv->fake), PAL_KERNEL);
    super_block_init(&priv->fake->sb);
    delay_work_init(&priv->fake->work);
    priv->fake->sb.ops = &fake_ops;

    priv->next_ino = 2;

    priv->temp_ptr_pages = palloc_va(5, PAL_KERNEL);

    return 0;
}

/* Reset the set of called functions on the super block */
static int inode_table_test_setup(struct ktest *kt)
{
    struct module_priv *priv = ktest_get_mod_priv(kt);
    priv->fake->called = 0;

    return 0;
}

static int inode_table_module_teardown(struct ktest_module *mod)
{
    struct module_priv *priv = mod->priv;

    pfree_va(priv->temp_ptr_pages, 5);
    kfree(priv->fake);
    kfree(priv);

    return 0;
}

static const struct ktest_unit inode_table_test_units[] = {
    KTEST_UNIT("test-mark-inode-bad-queue", test_mark_inode_bad_queue),
    KTEST_UNIT("test-mark-inode-valid--queue", test_mark_inode_valid_queue),
    KTEST_UNIT("test-inode-get-calls-inode-read", test_inode_get_calls_inode_read),
    KTEST_UNIT("test-inode-dirty-write-to-disk-calls-inode-write", test_inode_dirty_write_to_disk_calls_inode_write),
    KTEST_UNIT("test-inode-not-dirty-write-to-disk-does-nothing", test_inode_not_dirty_write_to_disk_does_nothing),
    KTEST_UNIT("test-inode-wait-for-write", test_inode_wait_for_write),
    KTEST_UNIT("test-inode-zero-refs-deleted", test_inode_zero_refs_deleted),
    KTEST_UNIT("test-inode-waiting-on-freeing", test_inode_wait_on_freeing),
    KTEST_UNIT("test-inode-dirty-put-adds-to-list", test_inode_dirty_put_adds_to_list),
    KTEST_UNIT("test-multiple-inode-get-returns-same-inode", test_multiple_inode_get_return_same_inode),
    KTEST_UNIT("test-inode-sync", test_inode_sync,
            (KT_INT(1)),
            (KT_INT(10)),
            (KT_INT(100)),
            (KT_INT(1000)),
            (KT_INT(10000)),
            (KT_INT(20000))),
    KTEST_UNIT("test-inode-sync-diff-super", test_inode_sync_diff_super,
            (KT_INT(1)),
            (KT_INT(10)),
            (KT_INT(100))),
    KTEST_UNIT("test-inode-sync-all", test_inode_sync_all,
            (KT_INT(1), KT_INT(1)),
            (KT_INT(1), KT_INT(2)),
            (KT_INT(1), KT_INT(4)),
            (KT_INT(1), KT_INT(8)),
            (KT_INT(20), KT_INT(1)),
            (KT_INT(20), KT_INT(2)),
            (KT_INT(20), KT_INT(4)),
            (KT_INT(20), KT_INT(8))),
};

KTEST_MODULE_DEFINE("inode-table", inode_table_test_units, inode_table_module_setup, inode_table_module_teardown, inode_table_test_setup, NULL);
