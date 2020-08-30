/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for slab.c - included directly at the end of slab.c
 */

#include <protura/types.h>
#include <protura/mm/kmalloc.h>
#include <protura/ktest.h>

static int slab_frame_alloc_count(struct slab_page_frame *frame)
{
    int count = 0;

    for (; frame; frame = frame->next)
        count += frame->object_count - frame->free_object_count;

    return count;
}

static void slab_test_frame_new(struct ktest *kt)
{
    int obj_size = 1024;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);
    struct slab_page_frame *new_frame;
    int obj_count = (1 << CONFIG_KERNEL_SLAB_ORDER) * PG_SIZE / obj_size - 1;

    spinlock_acquire(&test_slab.lock);
    int err = __slab_frame_add_new(&test_slab, PAL_KERNEL);
    spinlock_release(&test_slab.lock);

    ktest_assert_equal(kt, 0, err);

    new_frame = test_slab.first_frame;

    ktest_assert_notequal(kt, NULL, new_frame);
    ktest_assert_equal(kt, obj_count, new_frame->object_count);
    ktest_assert_equal(kt, obj_count, new_frame->free_object_count);

    struct page_frame_obj_empty *empty_list = new_frame->freelist;

    int free_count = 0;
    void *first_addr = new_frame->first_addr;

    for (; empty_list; empty_list = empty_list->next, first_addr += 1024, free_count++)
        ktest_assert_equal(kt, first_addr, empty_list);

    ktest_assert_equal(kt, obj_count, free_count);

    __slab_frame_free(&test_slab, new_frame);
    slab_clear(&test_slab);
}

static void slab_test_two_frames(struct ktest *kt)
{
    int obj_size = 512;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);
    struct page *pages = palloc(0, PAL_KERNEL);
    void **ptrs = pages->virt;
    int obj_count = (1 << CONFIG_KERNEL_SLAB_ORDER) * PG_SIZE / obj_size - 1;

    /* Allocate two frames worth of objects, verify the frames, and then free them */
    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    int i;
    for (i = 0; i < obj_count; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, obj_count, test_slab.first_frame->object_count);
    ktest_assert_equal(kt, 0, test_slab.first_frame->free_object_count);
    ktest_assert_equal(kt, NULL, test_slab.first_frame->next);

    for (i = 0; i < obj_count; i++)
        ptrs[i + obj_count] = slab_malloc(&test_slab, PAL_KERNEL);

    int frame_count = 0;
    struct slab_page_frame *frame;
    for (frame = test_slab.first_frame; frame; frame = frame->next, frame_count++) {
        ktest_assert_equal(kt, obj_count, frame->object_count);
        ktest_assert_equal(kt, 0, frame->free_object_count);
        ktest_assert_equal(kt, NULL, frame->freelist);
    }

    ktest_assert_equal(kt, 2, frame_count);

    for (i = 0; i < obj_count * 2; i++) {
        ktest_assert_equal(kt, 0, (uintptr_t)ptrs[i] & 0xFF);
        slab_free(&test_slab, ptrs[i]);
    }

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
    pfree(pages, 0);
}

static void slab_test_group_four_free_size(struct ktest *kt, int obj_size)
{
    void *ptrs[200];
    int i, k;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    /* Allocate 200 ptrs and free groups of four */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, 200, slab_frame_alloc_count(test_slab.first_frame));

    for (i = 0; i < 200; i++)
        ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 4; k++) {
        for (i = k; i < 50; i += 4) {
            slab_free(&test_slab, ptrs[i * 4]);
            slab_free(&test_slab, ptrs[i * 4 + 1]);
            slab_free(&test_slab, ptrs[i * 4 + 2]);
            slab_free(&test_slab, ptrs[i * 4 + 3]);
        }
    }

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
}

static void slab_test_group_two_free_size(struct ktest *kt, int obj_size)
{
    void *ptrs[200];
    int i, k;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    /* Allocate 200 ptrs and free groups of two */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, 200, slab_frame_alloc_count(test_slab.first_frame));

    for (i = 0; i < 200; i++)
        ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 10; k++) {
        for (i = k; i < 100; i += 10) {
            slab_free(&test_slab, ptrs[i * 2]);
            slab_free(&test_slab, ptrs[i * 2 + 1]);
        }
    }

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
}

static int slab_test_every_tenth_free_size(struct ktest *kt, int obj_size)
{
    int ret = 0;
    void *ptrs[200];
    int i, k;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    /* Allocate 200 ptrs and free every 10th */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, 200, slab_frame_alloc_count(test_slab.first_frame));

    for (i = 0; i < 200; i++)
        ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 10; k++)
        for (i = k; i < 200; i += 10)
            slab_free(&test_slab, ptrs[i]);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
    return ret;
}

static void slab_test_every_third_free_size(struct ktest *kt, int obj_size)
{
    void *ptrs[200];
    int i, k;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    /* Allocate 200 ptrs and free every third */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, 200, slab_frame_alloc_count(test_slab.first_frame));

    for (i = 0; i < 200; i++)
        ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 3; k++)
        for (i = k; i < 200; i += 3)
            slab_free(&test_slab, ptrs[i]);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
}

static void slab_test_reverse_free_size(struct ktest *kt, int obj_size)
{
    void *ptrs[200];
    int i;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    /* Allocate 200 ptrs and free them in reverse order */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, 200, slab_frame_alloc_count(test_slab.first_frame));

    for (i = 0; i < 200; i++)
        ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (i = 199; i >= 0; i--)
        slab_free(&test_slab, ptrs[i]);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
}

static void slab_test_inorder_free_size(struct ktest *kt, int obj_size)
{
    void *ptrs[200];
    int i;
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    /* Allocate 200 ptrs and free them in order */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    ktest_assert_notequal(kt, NULL, test_slab.first_frame);
    ktest_assert_equal(kt, 200, slab_frame_alloc_count(test_slab.first_frame));

    for (i = 0; i < 200; i++)
        ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (i = 0; i < 200; i++)
        slab_free(&test_slab, ptrs[i]);

    /* After freeing every point in the slab, the entire frame should have been released */
    ktest_assert_equal(kt, NULL, test_slab.first_frame);

    slab_clear(&test_slab);
}

static void slab_test_inorder_free(struct ktest *kt)
{
    slab_test_inorder_free_size(kt, KT_ARG(kt, 0, int));
}

static void slab_test_reverse_free(struct ktest *kt)
{
    slab_test_reverse_free_size(kt, KT_ARG(kt, 0, int));
}

static void slab_test_every_third_free(struct ktest *kt)
{
    slab_test_every_third_free_size(kt, KT_ARG(kt, 0, int));
}

static void slab_test_every_tenth_free(struct ktest *kt)
{
    slab_test_every_tenth_free_size(kt, KT_ARG(kt, 0, int));
}

static void slab_test_group_two_free(struct ktest *kt)
{
    slab_test_group_two_free_size(kt, KT_ARG(kt, 0, int));
}

static void slab_test_group_four_free(struct ktest *kt)
{
    slab_test_group_four_free_size(kt, KT_ARG(kt, 0, int));
}

#define SLAB_TEST_CASES(st, func) \
    KTEST_UNIT(st, func, \
        (KT_INT(32)), \
        (KT_INT(64)), \
        (KT_INT(128)), \
        (KT_INT(256)), \
        (KT_INT(512)), \
        (KT_INT(1024)), \
        (KT_INT(2048)), \
        (KT_INT(4096)))

static const struct ktest_unit slab_test_units[] = {
    SLAB_TEST_CASES("free-inorder",     slab_test_inorder_free),
    SLAB_TEST_CASES("free-reverse",     slab_test_reverse_free),
    SLAB_TEST_CASES("free-every-third", slab_test_every_third_free),
    SLAB_TEST_CASES("free-every-tenth", slab_test_every_tenth_free),
    SLAB_TEST_CASES("free-group-two",   slab_test_group_two_free),
    SLAB_TEST_CASES("free-group-four",  slab_test_group_four_free),
    KTEST_UNIT("fill-two-frames",       slab_test_two_frames),
    KTEST_UNIT("new-frame",             slab_test_frame_new),
};

KTEST_MODULE_DEFINE("slab", slab_test_units);
