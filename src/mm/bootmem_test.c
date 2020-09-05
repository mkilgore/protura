/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for bootmem.c - included directly at the end of bootmem.c
 */

#include <protura/types.h>
#include <protura/mm/kmalloc.h>
#include <protura/ktest.h>

static void test_add_all_regions(struct ktest *kt)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(memregions); i++) {
        int err = bootmem_add(0x10000 + i * PG_SIZE, 0x11000 + i * PG_SIZE);
        ktest_assert_equal(kt, 0, err);
    }
}

static void test_extra_regions_discarded(struct ktest *kt)
{
    int i, err;
    for (i = 0; i < ARRAY_SIZE(memregions); i++) {
        err = bootmem_add(0x10000 + i * PG_SIZE, 0x11000 + i * PG_SIZE);
        ktest_assert_equal(kt, 0, err);
    }

    err = bootmem_add(0x40000 + i * PG_SIZE, 0x41000 + i * PG_SIZE);
    ktest_assert_equal(kt, -ENOMEM, err);
}

static void test_allocate_pages_successfully(struct ktest *kt)
{
    void *ptr;
    const int page_count = KT_ARG(kt, 0, int);
    int err = bootmem_add(0x10000, 0x10000 + PG_SIZE * page_count);
    ktest_assert_equal(kt, 0, err);

    int i;
    for (i = 0; i < page_count; i++) {
        ptr = bootmem_alloc_nopanic(PG_SIZE, PG_SIZE);
        ktest_assert_notequal(kt, NULL, ptr);

        pa_t phys = V2P(ptr);
        ktest_assert_equal(kt, 0x10000 + i * PG_SIZE, phys);
    }

    ptr = bootmem_alloc_nopanic(1, 1);
    ktest_assert_equal(kt, NULL, ptr);
}

static void test_allocate_bytes_successfully(struct ktest *kt)
{
    void *ptr;
    const int byte_count = KT_ARG(kt, 0, int);
    int err = bootmem_add(0x10000, 0x10000 + byte_count);
    ktest_assert_equal(kt, 0, err);

    int i;
    for (i = 0; i < byte_count; i++) {
        ptr = bootmem_alloc_nopanic(1, 1);
        ktest_assert_notequal(kt, NULL, ptr);

        pa_t phys = V2P(ptr);
        ktest_assert_equal(kt, 0x10000 + i, phys);
    }

    ptr = bootmem_alloc_nopanic(1, 1);
    ktest_assert_equal(kt, NULL, ptr);
}

static void test_kernel_region_excluded_split(struct ktest *kt)
{
    kernel_start_address = P2V(0x2000);
    kernel_end_address = P2V(0x3000);

    bootmem_add(0x1000, 0x4000);

    ktest_assert_equal(kt, 0x1000, memregions[0].start);
    ktest_assert_equal(kt, 0x2000, memregions[0].end);
    ktest_assert_equal(kt, 0x3000, memregions[1].start);
    ktest_assert_equal(kt, 0x4000, memregions[1].end);

    void *ptr = bootmem_alloc_nopanic(PG_SIZE, PG_SIZE);
    ktest_assert_notequal(kt, NULL, ptr);
    ktest_assert_equal(kt, 0x1000, V2P(ptr));

    ptr = bootmem_alloc_nopanic(PG_SIZE, PG_SIZE);
    ktest_assert_notequal(kt, NULL, ptr);
    ktest_assert_equal(kt, 0x3000, V2P(ptr));
}

static void test_kernel_region_excluded_after(struct ktest *kt)
{
    kernel_start_address = P2V(0x2000);
    kernel_end_address = P2V(0x4000);

    bootmem_add(0x3000, 0x7000);

    ktest_assert_equal(kt, 0x4000, memregions[0].start);
    ktest_assert_equal(kt, 0x7000, memregions[0].end);

    int i;
    for (i = 0; i < 3; i++) {
        void *ptr = bootmem_alloc_nopanic(PG_SIZE, PG_SIZE);
        ktest_assert_notequal(kt, NULL, ptr);
        ktest_assert_equal(kt, 0x4000 + i * 0x1000, V2P(ptr));
    }
}

static void test_kernel_region_excluded_before(struct ktest *kt)
{
    kernel_start_address = P2V(0x3000);
    kernel_end_address = P2V(0x5000);

    bootmem_add(0x1000, 0x4000);

    ktest_assert_equal(kt, 0x1000, memregions[0].start);
    ktest_assert_equal(kt, 0x3000, memregions[0].end);

    int i;
    for (i = 0; i < 2; i++) {
        void *ptr = bootmem_alloc_nopanic(PG_SIZE, PG_SIZE);
        ktest_assert_notequal(kt, NULL, ptr);
        ktest_assert_equal(kt, 0x1000 + i * 0x1000, V2P(ptr));
    }
}

static int bootmem_test_setup(struct ktest *kt)
{
    /* The memory regions are static, but since by the time we run the tests
     * we're long past the booting stage we can just clear and reuse these for
     * every test */
    memset(memregions, 0, sizeof(memregions));
    highest_page = 0;

    kernel_end_address = NULL;
    kernel_start_address = NULL;

    return 0;
}

static const struct ktest_unit bootmem_test_units[] = {
    KTEST_UNIT("add-all-regions", test_add_all_regions),
    KTEST_UNIT("extra-regions-discarded", test_extra_regions_discarded),
    KTEST_UNIT("allocate-pages-successfully", test_allocate_pages_successfully,
            (KT_INT(1)),
            (KT_INT(10)),
            (KT_INT(20)),
            (KT_INT(200)),
            (KT_INT(4000))),

    /* Regions are page-aligned, so only so many values are possible here */
    KTEST_UNIT("allocate-bytes-successfully", test_allocate_bytes_successfully,
            (KT_INT(PG_SIZE)),
            (KT_INT(PG_SIZE * 2)),
            (KT_INT(PG_SIZE * 10)),
            (KT_INT(PG_SIZE * 20)),
            (KT_INT(PG_SIZE * 50))),

    KTEST_UNIT("test-kernel-region-excluded-split", test_kernel_region_excluded_split),
    KTEST_UNIT("test-kernel-region-excluded-after", test_kernel_region_excluded_after),
    KTEST_UNIT("test-kernel-region-excluded-before", test_kernel_region_excluded_before),
};

KTEST_MODULE_DEFINE("bootmem", bootmem_test_units, NULL, NULL, bootmem_test_setup, NULL);
