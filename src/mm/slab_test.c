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

static int slab_test_size(struct ktest *kt, int obj_size)
{
    int ret = 0;
    void *ptrs[200];
    struct slab_alloc test_slab = SLAB_ALLOC_INIT("test-slab", obj_size);

    int i, k;

    /* Allocate 200 ptrs and free them in order */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    for (i = 0; i < 200; i++)
        ret += ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (i = 0; i < 200; i++)
        slab_free(&test_slab, ptrs[i]);

    /* Allocate 200 ptrs and free them in reverse order */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    for (i = 0; i < 200; i++)
        ret += ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (i = 199; i >= 0; i--)
        slab_free(&test_slab, ptrs[i]);

    /* Allocate 200 ptrs and free every third */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    for (i = 0; i < 200; i++)
        ret += ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 3; k++)
        for (i = k; i < 200; i += 3)
            slab_free(&test_slab, ptrs[i]);

    /* Allocate 200 ptrs and free every 10th */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    for (i = 0; i < 200; i++)
        ret += ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 10; k++)
        for (i = k; i < 200; i += 10)
            slab_free(&test_slab, ptrs[i]);

    /* Allocate 200 ptrs and free groups of two */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    for (i = 0; i < 200; i++)
        ret += ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 10; k++) {
        for (i = k; i < 100; i += 10) {
            slab_free(&test_slab, ptrs[i * 2]);
            slab_free(&test_slab, ptrs[i * 2 + 1]);
        }
    }

    /* Allocate 200 ptrs and free groups of five */
    for (i = 0; i < 200; i++)
        ptrs[i] = slab_malloc(&test_slab, PAL_KERNEL);

    for (i = 0; i < 200; i++)
        ret += ktest_assert_equal(kt, 0, slab_has_addr(&test_slab, ptrs[i]));

    for (k = 0; k < 4; k++) {
        for (i = k; i < 50; i += 4) {
            slab_free(&test_slab, ptrs[i * 4]);
            slab_free(&test_slab, ptrs[i * 4 + 1]);
            slab_free(&test_slab, ptrs[i * 4 + 2]);
            slab_free(&test_slab, ptrs[i * 4 + 3]);
        }
    }

    slab_clear(&test_slab);

    return ret;
}

static int slab_test(struct ktest *kt)
{
    int ret = 0;

    int test_cases[] = { 32, 64, 128, 256, 512, 1024, 2048, 4096 };

    int i;
    for (i = 0; i < ARRAY_SIZE(test_cases); i++)
        ret += slab_test_size(kt, test_cases[i]);

    return ret;
}

static const struct ktest_unit slab_test_units[] = {
    KTEST_UNIT_INIT("slab_test", slab_test),
};

static const struct ktest_module __ktest slab_test_module = {
    .name = "slab",
    .tests = slab_test_units,
    .test_count = ARRAY_SIZE(slab_test_units),
};
