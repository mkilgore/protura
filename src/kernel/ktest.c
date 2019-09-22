/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/snprintf.h>
#include <protura/string.h>
#include <protura/cmdline.h>
#include <protura/ktest.h>

#include <arch/reset.h>

struct ktest {
    int cur_unit_test;
    int cur_test;
};

extern struct ktest_module __ktest_start;
extern struct ktest_module __ktest_end;

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
        ktest.cur_test = 0;
        ktest.cur_unit_test++;

        kp(KP_NORMAL, "== #%d: %s ==\n", i, tests[i].name);

        int errs = (tests + i)->test (&ktest);

        if (errs != 0)
            snprintf(buf, sizeof(buf), "FAIL -> %d", errs);
        else
            snprintf(buf, sizeof(buf), "PASS");

        kp(KP_NORMAL, "== Result: %s ==\n", buf);

        error_count += errs;
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
    struct ktest_module *start = &__ktest_start;
    struct ktest_module *end = &__ktest_end;

    for (; start < end; start++)
        errs += run_module(start);

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

#if 0
static void ktest_value_show(const char *prefix, const char *asdf, struct ktest_value *v)
{

}
#endif

int ktest_assert_equal_value_func(struct ktest *ktest, struct ktest_value *expected, struct ktest_value *actual, const char *func, int lineno)
{
    char buf[255];
    ktest->cur_test++;

    int result = ktest_value_comp(expected, actual);

    if (result)
        snprintf(buf, sizeof(buf), "PASS");
    else
        snprintf(buf, sizeof(buf), "FAIL");

    if (!result)
        kp(KP_NORMAL, " [%02d:%03d] %s: %d: %s == %s: %s\n", ktest->cur_unit_test, ktest->cur_test, func, lineno, expected->value_string, actual->value_string, buf);

    return !result;
}

int ktest_assert_notequal_value_func(struct ktest *ktest, struct ktest_value *expected, struct ktest_value *actual, const char *func, int lineno)
{
    char buf[255];
    ktest->cur_test++;

    int result = ktest_value_comp(expected, actual) == 0;

    if (result)
        snprintf(buf, sizeof(buf), "PASS");
    else
        snprintf(buf, sizeof(buf), "FAIL");

    if (!result)
        kp(KP_NORMAL, " [%02d:%03d] %s: %d: %s != %s: %s\n", ktest->cur_unit_test, ktest->cur_test, func, lineno, expected->value_string, actual->value_string, buf);

    return !result;
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

