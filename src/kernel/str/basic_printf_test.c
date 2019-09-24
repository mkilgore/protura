/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for basic_printf.c - included directly at the end of basic_printf.c
 */

#include <protura/types.h>
#include <protura/snprintf.h>
#include <protura/ktest.h>

struct ktest_printf_backbone {
    struct printf_backbone backbone;
    int putchar_was_called;
    int putnstr_was_called;
};

static void test_add_str_putnstr_case(struct ktest *kt, const char *in, size_t in_len, const char *out, size_t out_len)
{
    void test_putchar(struct printf_backbone *back, char ch)
    {
        struct ktest_printf_backbone *ktest_backbone = container_of(back, struct ktest_printf_backbone, backbone);
        ktest_backbone->putchar_was_called++;
    }

    void test_putnstr(struct printf_backbone *back, const char *str, size_t len)
    {
        struct ktest_printf_backbone *ktest_backbone = container_of(back, struct ktest_printf_backbone, backbone);
        ktest_backbone->putnstr_was_called++;
        ktest_assert_equal(kt, len, out_len);
        ktest_assert_equal_mem(kt, out, str, len);
    }

    struct ktest_printf_backbone backbone = {
        .backbone = PRINTF_BACKBONE(test_putchar, test_putnstr),
    };

    basic_printf_add_str(&backbone.backbone, in, in_len);

    ktest_assert_equal(kt, 0, backbone.putchar_was_called);
    ktest_assert_equal(kt, 1, backbone.putnstr_was_called);
}

static void test_add_str(struct ktest *kt)
{
    test_add_str_putnstr_case(kt, "test", 4, "test", 4);
    test_add_str_putnstr_case(kt, "test", 2, "te", 2);
    test_add_str_putnstr_case(kt, NULL, 2, "(null)", 6);
    test_add_str_putnstr_case(kt, NULL, 100, "(null)", 6);
    test_add_str_putnstr_case(kt, NULL, 2, "(null)", 6);
}

static void test_int_parse(struct ktest *kt)
{
    void test_putchar(struct printf_backbone *back, char ch)
    {
        struct ktest_printf_backbone *ktest_backbone = container_of(back, struct ktest_printf_backbone, backbone);
        ktest_backbone->putchar_was_called++;
    }

    void test_putnstr(struct printf_backbone *back, const char *str, size_t len)
    {
        struct ktest_printf_backbone *ktest_backbone = container_of(back, struct ktest_printf_backbone, backbone);
        ktest_backbone->putnstr_was_called++;
        ktest_assert_notequal(kt, NULL, (void *)str);
        ktest_assert_equal(kt, 1, len);
        ktest_assert_equal_mem(kt, str, "1", 1);
    }

    struct ktest_printf_backbone backbone = {
        .backbone = PRINTF_BACKBONE(test_putchar, test_putnstr),
    };

    basic_printf(&backbone.backbone, "%d", 1);

    ktest_assert_equal(kt, 0, backbone.putchar_was_called);
    ktest_assert_equal(kt, 1, backbone.putnstr_was_called);
}

static const struct ktest_unit basic_printf_test_units[] = {
    KTEST_UNIT_INIT("add-str", test_add_str),
    KTEST_UNIT_INIT("int-parse", test_int_parse),
};

static const struct ktest_module __ktest basic_printf_test_module = {
    .name = "basic_printf",
    .tests = basic_printf_test_units,
    .test_count = ARRAY_SIZE(basic_printf_test_units),
};
