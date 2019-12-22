/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for char_buf.c - included directly at the end of char_buf.c
 */

#include <protura/types.h>
#include <protura/char_buf.h>
#include <protura/mm/palloc.h>
#include <protura/ktest.h>

static void char_buf_read_wrap_test(struct ktest *kt)
{
    struct page *page = palloc(0, PAL_KERNEL);
    struct char_buf buf;
    char_buf_init(&buf, page->virt, PG_SIZE);

    char tmp[128];
    int i;
    for (i = 0; i < ARRAY_SIZE(tmp); i++)
        tmp[i] = i;

    buf.start_pos = PG_SIZE - 64;
    buf.buf_len = 128;
    memcpy(page->virt + PG_SIZE - 64, tmp, 64);
    memcpy(page->virt, tmp + 64, 64);

    char read_ret[128] = { 0 };
    size_t ret = char_buf_read(&buf, read_ret, sizeof(read_ret));

    ktest_assert_equal(kt, 128, ret);
    ktest_assert_equal(kt, 64, buf.start_pos);
    ktest_assert_equal(kt, 0, buf.buf_len);
    ktest_assert_equal_mem(kt, tmp, read_ret, 128);

    pfree(page, 0);
}

static void char_buf_read_user_wrap_test(struct ktest *kt)
{
    struct page *page = palloc(0, PAL_KERNEL);
    struct char_buf buf;
    char_buf_init(&buf, page->virt, PG_SIZE);

    char tmp[128];
    int i;
    for (i = 0; i < ARRAY_SIZE(tmp); i++)
        tmp[i] = i;

    buf.start_pos = PG_SIZE - 64;
    buf.buf_len = 128;
    memcpy(page->virt + PG_SIZE - 64, tmp, 64);
    memcpy(page->virt, tmp + 64, 64);

    char read_ret[128] = { 0 };
    size_t ret = char_buf_read_user(&buf, make_kernel_buffer(read_ret), sizeof(read_ret));

    ktest_assert_equal(kt, 128, ret);
    ktest_assert_equal(kt, 64, buf.start_pos);
    ktest_assert_equal(kt, 0, buf.buf_len);
    ktest_assert_equal_mem(kt, tmp, read_ret, 128);

    pfree(page, 0);
}

static void char_buf_write_wrap_test(struct ktest *kt)
{
    struct page *page = palloc(0, PAL_KERNEL);
    struct char_buf buf;
    char_buf_init(&buf, page->virt, PG_SIZE);

    char tmp[128];
    int i;
    for (i = 0; i < ARRAY_SIZE(tmp); i++)
        tmp[i] = i;

    buf.start_pos = PG_SIZE - 64;

    char_buf_write(&buf, tmp, sizeof(tmp));

    ktest_assert_equal(kt, PG_SIZE - 64, buf.start_pos);
    ktest_assert_equal(kt, sizeof(tmp), buf.buf_len);

    ktest_assert_equal_mem(kt, tmp, page->virt + PG_SIZE - 64, 64);
    ktest_assert_equal_mem(kt, tmp + 64, page->virt, 64);

    pfree(page, 0);
}

static void char_buf_write_test(struct ktest *kt)
{
    struct page *page = palloc(0, PAL_KERNEL);
    struct char_buf buf;
    char_buf_init(&buf, page->virt, PG_SIZE);

    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, 0, buf.buf_len);

    char tmp[128];
    int i;
    for (i = 0; i < ARRAY_SIZE(tmp); i++)
        tmp[i] = i;

    char_buf_write(&buf, tmp, sizeof(tmp));
    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, sizeof(tmp), buf.buf_len);

    char_buf_write(&buf, tmp, sizeof(tmp));
    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, sizeof(tmp) * 2, buf.buf_len);

    char_buf_write(&buf, tmp, sizeof(tmp));
    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, sizeof(tmp) * 3, buf.buf_len);

    char_buf_write(&buf, tmp, sizeof(tmp));
    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, sizeof(tmp) * 4, buf.buf_len);

    char_buf_write(&buf, tmp, sizeof(tmp));
    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, sizeof(tmp) * 5, buf.buf_len);

    pfree(page, 0);
}

static void char_buf_empty_read_test(struct ktest *kt)
{
    struct page *page = palloc(0, PAL_KERNEL);
    struct char_buf buf;
    char_buf_init(&buf, page->virt, PG_SIZE);

    ktest_assert_equal(kt, 0, buf.start_pos);
    ktest_assert_equal(kt, 0, buf.buf_len);

    char tmp[10] = { 0 };
    size_t ret = char_buf_read(&buf, tmp, sizeof(tmp));
    ktest_assert_equal(kt, 0, ret);

    pfree(page, 0);
}

static const struct ktest_unit char_buf_test_units[] = {
    KTEST_UNIT("char-buf-read-wrap-test", char_buf_read_wrap_test),
    KTEST_UNIT("char-buf-read-user-wrap-test", char_buf_read_user_wrap_test),
    KTEST_UNIT("char-buf-write-wrap-test", char_buf_write_wrap_test),
    KTEST_UNIT("char-buf-write-test", char_buf_write_test),
    KTEST_UNIT("char-buf-empty-read-test", char_buf_empty_read_test),
};

static const __ktest struct ktest_module char_buf_test_module
    = KTEST_MODULE_INIT("char-buf", char_buf_test_units);
