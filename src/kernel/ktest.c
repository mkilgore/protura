/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/bits.h>
#include <protura/dump_mem.h>
#include <protura/snprintf.h>
#include <protura/string.h>
#include <protura/cmdline.h>
#include <protura/ksetjmp.h>
#include <protura/ktest.h>

#include <arch/reset.h>

struct ktest {
    int cur_unit_test;
    int cur_test;

    const struct ktest_unit *unit;
    int failed_tests;

    /* 
     * We use ksetjmp and klongjmp to exit a test when an assertion fails
     *
     * The logic is that if an assertion fails subsequent code may crash the
     * kernel, such as if it attempts to deref a pointer it just asserted was
     * not NULL. Because of this, we will longjmp out of the test on an
     * assertion failure, so we can keep running tests.
     *
     * Note: This does mean test resources may not be cleaned up, such as
     * memory not freed and such. This is a known but accepted problem, since
     * it's only a problem for test code.
     */
    kjmpbuf_t ktest_assert_fail;
};

extern struct ktest_module __ktest_start;
extern struct ktest_module __ktest_end;

const struct ktest_arg *ktest_get_arg(struct ktest *kt, int index)
{
    if (index >= kt->unit->arg_count)
        ktest_assert_fail(kt, "Arg index %d does not exist, total args: %d\n", index, kt->unit->arg_count);

    return kt->unit->args + index;
}

/*
 * noinline is necessary to ensure the code calling ksetjmp isn't inlined incorrectly
 */
static noinline void run_test(struct ktest *ktest)
{
    if (ksetjmp(&ktest->ktest_assert_fail) == 0)
        ktest->unit->test(ktest);
}

static int run_module(struct ktest_module *module)
{
    char buf[256];
    const struct ktest_unit *tests = module->tests;
    struct ktest ktest;
    memset(&ktest, 0, sizeof(ktest));
    int error_count = 0;

    kp(KP_NORMAL, "==== Starting tests for %s ====\n", module->name);

    int i;
    for (i = 0; i < module->test_count; i++) {
        char arg_str[32];

        ktest.cur_test = 0;
        ktest.cur_unit_test++;
        ktest.failed_tests = 0;
        ktest.unit = tests + i;

        arg_str[0] = '\0';
        if (ktest.unit->arg_count) {
            strcat(arg_str, "(");
            size_t arg_str_off = 1;
            int k;
            for (k = 0; k < ktest.unit->arg_count; k++) {
                arg_str_off += snprintf(arg_str + arg_str_off, sizeof(arg_str) - arg_str_off, "%s", ktest.unit->args[k].name);

                if (k + 1 != ktest.unit->arg_count)
                    arg_str_off += snprintf(arg_str + arg_str_off, sizeof(arg_str) - arg_str_off, ",");

            }
            arg_str_off += snprintf(arg_str + arg_str_off, sizeof(arg_str) - arg_str_off, ")");
        }

        kp(KP_NORMAL, "== #%d: %s%s ==\n", i, tests[i].name, arg_str);

        run_test(&ktest);

        if (ktest.failed_tests != 0)
            snprintf(buf, sizeof(buf), "FAIL -> %d", ktest.failed_tests);
        else
            snprintf(buf, sizeof(buf), "PASS");

        kp(KP_NORMAL, "== Result: %s ==\n", buf);

        error_count += ktest.failed_tests;
    }

    kp(KP_NORMAL, "==== Finished tests for %s ====\n", module->name);

    if (error_count == 0)
        snprintf(buf, sizeof(buf), "PASS");
    else
        snprintf(buf, sizeof(buf), "FAIL -> %d ", error_count);
    kp(KP_NORMAL, "==== Result: %s ====\n", buf);

    return error_count;
}

static void run_ktest_modules(void)
{
    int errs = 0;
    int total_tests = 0;
    struct ktest_module *start = &__ktest_start;
    struct ktest_module *end = &__ktest_end;

    for (; start < end; start++) {
        errs += run_module(start);
        total_tests += start->test_count;
    }

    kp(KP_NORMAL, "==== Total test count: %d ====\n", total_tests);

    char buf[255];

    if (!errs)
        snprintf(buf, sizeof(buf), "PASS");
    else
        snprintf(buf, sizeof(buf), "FAIL -> %d", errs);

    kp(KP_NORMAL, "==== Full test run: %s ====\n", buf);
}


static int ktest_value_comp(struct ktest_value *v1, struct ktest_value *v2)
{
    if (v1->type != v2->type)
        return 1;

    switch (v1->type) {
    case KTEST_VALUE_INT64:
        return v1->int64 == v2->int64;

    case KTEST_VALUE_UINT64:
        return v1->uint64 == v2->uint64;

    case KTEST_VALUE_PTR:
        return v1->ptr == v2->ptr;

    case KTEST_VALUE_STR:
        return !strcmp(v1->ptr, v2->ptr);

    case KTEST_VALUE_MEM:
        return !memcmp(v1->ptr, v2->ptr, v1->len);
    }

    return 1;
}

static void ktest_value_show(const char *prefix, const char *name, struct ktest_value *v)
{
    char value_buf[128];

    switch (v->type) {
    case KTEST_VALUE_INT64:
        snprintf(value_buf, sizeof(value_buf), "%lld", v->int64);
        break;

    case KTEST_VALUE_UINT64:
        snprintf(value_buf, sizeof(value_buf), "%llu", v->int64);
        break;

    case KTEST_VALUE_PTR:
        snprintf(value_buf, sizeof(value_buf), "%p", v->ptr);
        break;

    case KTEST_VALUE_STR:
        snprintf(value_buf, sizeof(value_buf), "\"%s\"", v->ptr);
        break;

    case KTEST_VALUE_MEM:
        kp(KP_NORMAL, "  %s %s:\n", name, prefix);
        dump_mem(v->ptr, v->len, (uint32_t)v->ptr);
        return;
    }

    kp(KP_NORMAL, "  %s %s: %s\n", name, prefix, value_buf);
}

void ktest_assert_equal_value_func(struct ktest *ktest, struct ktest_value *expected, struct ktest_value *actual, const char *func, int lineno)
{
    char buf[64];
    ktest->cur_test++;

    int result = ktest_value_comp(expected, actual);

    if (result)
        snprintf(buf, sizeof(buf), "PASS");
    else
        snprintf(buf, sizeof(buf), "FAIL");

    ktest->failed_tests += !result;

    if (!result) {
        kp(KP_NORMAL, " [%02d:%03d] %s: %d: %s == %s: %s\n", ktest->cur_unit_test, ktest->cur_test, func, lineno, expected->value_string, actual->value_string, buf);
        ktest_value_show("expected", actual->value_string, expected);
        ktest_value_show("actual  ", actual->value_string, actual);

        klongjmp(&ktest->ktest_assert_fail, 1);
    }
}

void ktest_assert_notequal_value_func(struct ktest *ktest, struct ktest_value *expected, struct ktest_value *actual, const char *func, int lineno)
{
    char buf[64];
    ktest->cur_test++;

    int result = ktest_value_comp(expected, actual) == 0;

    if (result)
        snprintf(buf, sizeof(buf), "PASS");
    else
        snprintf(buf, sizeof(buf), "FAIL");

    ktest->failed_tests += !result;

    if (!result) {
        kp(KP_NORMAL, " [%02d:%03d] %s: %d: %s != %s: %s\n", ktest->cur_unit_test, ktest->cur_test, func, lineno, expected->value_string, actual->value_string, buf);
        ktest_value_show("actual", actual->value_string, actual);

        klongjmp(&ktest->ktest_assert_fail, 1);
    }
}

void ktest_assert_failv(struct ktest *kt, const char *fmt, va_list lst)
{
    char buf[256];

    snprintfv(buf, sizeof(buf), fmt, lst);

    kp(KP_NORMAL, " [%02d:%03d]: FAIL\n", kt->cur_unit_test, kt->cur_test);
    kp(KP_NORMAL, "  %s", buf);

    kt->failed_tests += 1;
    klongjmp(&kt->ktest_assert_fail, 1);
}

void ktest_assert_fail(struct ktest *kt, const char *fmt, ...)
{
    va_list lst;

    va_start(lst, fmt);
    ktest_assert_failv(kt, fmt, lst);
    va_end(lst);
}

void ktest_init(void)
{
    int run_tests = kernel_cmdline_get_bool("ktest.run", 0);

    if (!run_tests)
        return;

    int reboot_after_tests = kernel_cmdline_get_bool("ktests.reboot_after_run", 0);
    if (reboot_after_tests)
        reboot_on_panic = 1;

    run_ktest_modules();

    if (reboot_after_tests)
        system_reboot();
}

