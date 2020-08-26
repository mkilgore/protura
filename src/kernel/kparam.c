/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/cmdline.h>
#include <protura/kparam.h>

extern struct kparam __kparam_start, __kparam_end;

static void kparam_parse_bool(struct kparam *param)
{
    int val;

    int err = kernel_cmdline_get_bool_nodef(param->name, &val);
    if (err)
        return;

    *(int *)(param->param) = val;

    if (param->setup)
        (param->setup) (param);
}

static void kparam_parse_string(struct kparam *param)
{
    const char *val;

    int err = kernel_cmdline_get_string_nodef(param->name, &val);
    if (err)
        return;

    *(const char **)(param->param) = val;

    if (param->setup)
        (param->setup) (param);
}

static void kparam_parse_int(struct kparam *param)
{
    int val;

    int err = kernel_cmdline_get_int_nodef(param->name, &val);
    if (err)
        return;

    *(int *)(param->param) = val;

    if (param->setup)
        (param->setup) (param);
}

void kparam_init(void)
{
    struct kparam *param = &__kparam_start;

    for (; param < &__kparam_end; param++) {
        switch (param->type) {
        case KPARAM_BOOL:
            kparam_parse_bool(param);
            break;

        case KPARAM_INT:
            kparam_parse_int(param);
            break;

        case KPARAM_STRING:
            kparam_parse_string(param);
            break;
        }
    }
}
