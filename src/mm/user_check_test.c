/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for user_check.c - included directly at the end of user_check.c
 */

#include <protura/types.h>
#include <protura/mm/user_check.h>
#include <protura/ktest.h>

static void user_check_copy_from_kernel_test(struct ktest *kt)
{
    const int max_len = 256;
    size_t len = KT_ARG(kt, 0, int);

    /* We hard-code 256 to ensure the stack does not overflow */
    ktest_assert_notequal(kt, 0, len <= max_len);

    char kbuf[max_len];
    char ubuf[max_len];

    size_t i;
    for (i = 0; i < sizeof(kbuf); i++)
        kbuf[i] = i & 0xFF;

    memset(ubuf, 0, sizeof(ubuf));

    int err = user_memcpy_from_kernel(make_kernel_buffer(ubuf), kbuf, len);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal_mem(kt, kbuf, ubuf, len);
}

static void user_check_copy_to_kernel_test(struct ktest *kt)
{
    const int max_len = 256;
    size_t len = KT_ARG(kt, 0, int);

    /* We hard-code 256 to ensure the stack does not overflow */
    ktest_assert_notequal(kt, 0, len <= max_len);

    char kbuf[max_len];
    char ubuf[max_len];

    size_t i;
    for (i = 0; i < sizeof(ubuf); i++)
        ubuf[i] = i & 0xFF;

    memset(kbuf, 0, sizeof(kbuf));

    int err = user_memcpy_to_kernel(kbuf, make_kernel_buffer(ubuf), len);

    ktest_assert_equal(kt, 0, err);
    ktest_assert_equal_mem(kt, ubuf, kbuf, len);
}

static const struct ktest_unit user_check_test_units[] = {
    KTEST_UNIT("user-check-copy-from-kernel-test", user_check_copy_from_kernel_test,
            (KT_INT(1)),
            (KT_INT(2)),
            (KT_INT(3)),
            (KT_INT(4)),
            (KT_INT(8)),
            (KT_INT(16)),
            (KT_INT(32)),
            (KT_INT(64)),
            (KT_INT(256))),
    KTEST_UNIT("user-check-copy-to-kernel-test", user_check_copy_to_kernel_test,
            (KT_INT(1)),
            (KT_INT(2)),
            (KT_INT(3)),
            (KT_INT(4)),
            (KT_INT(8)),
            (KT_INT(16)),
            (KT_INT(32)),
            (KT_INT(64)),
            (KT_INT(256))),
};

KTEST_MODULE_DEFINE("user_check", user_check_test_units);
