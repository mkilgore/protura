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
#include <arch/init.h>
#include <protura/cmdline.h>

struct cmd_arg {
    const char *name;
    const char *value;
};

static int arg_count = 0;
static struct cmd_arg cmd_args[64];

static int is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

enum parse_state {
    STATE_ARG_BEGIN,
    STATE_ARG_EQUALS,
    STATE_VALUE_BEGIN,
    STATE_ARG_END,
};

static void add_arg(const char *name, const char *value)
{
    if (arg_count >= ARRAY_SIZE(cmd_args)) {
        kp(KP_WARNING, "To many args, ignoring arg: %s, value: %s\n", name, value);
        return;
    }

    kp(KP_NORMAL, "Kernel arg: %s=%s\n", name, value);

    cmd_args[arg_count].name = name;
    cmd_args[arg_count].value = value;
    arg_count++;
}

void kernel_cmdline_init(void)
{
    char *l = kernel_cmdline;
    const char *tmp_name = NULL;
    const char *tmp_value = NULL;
    enum parse_state state = STATE_ARG_BEGIN;

    for (; *l; l++) {
        switch (state) {
        case STATE_ARG_BEGIN:
            if (!is_whitespace(*l)) {
                tmp_name = l;
                state = STATE_ARG_EQUALS;
            }
            break;

        case STATE_ARG_EQUALS:
            if (*l == '=') {
                /* This marks the end of `tmp_name` and turns it into a NUL
                 * terminated string */
                *l = '\0';
                state = STATE_VALUE_BEGIN;
            }

            if (is_whitespace(*l)) {
                /* This arg has no value, ignore it for now */
                tmp_name = NULL;
                state = STATE_ARG_BEGIN;
            }
            break;

        case STATE_VALUE_BEGIN:
            if (is_whitespace(*l)) {
                /* value is empty */
                tmp_value = "";
                add_arg(tmp_name, tmp_value);
                state = STATE_ARG_BEGIN;
            } else {
                tmp_value = l;
                state = STATE_ARG_END;
            }
            break;

        case STATE_ARG_END:
            if (is_whitespace(*l)) {
                *l = '\0';
                add_arg(tmp_name, tmp_value);

                tmp_name = NULL;
                tmp_value = NULL;
                state = STATE_ARG_BEGIN;
            }
            break;
        }
    }

    if (state == STATE_ARG_END) {
        /* Add the last argument - it's already NUL terminated */
        add_arg(tmp_name, tmp_value);
    }
}

static struct cmd_arg *find_arg(const char *name)
{
    int i;
    for (i = 0; i < arg_count; i++)
        if (strcmp(cmd_args[i].name, name) == 0)
            return cmd_args + i;

    return NULL;
}

static int parse_bool(const char *value)
{
    if (strcasecmp(value, "true") == 0)
        return 1;
    else if (strcasecmp(value, "false") == 0)
        return 0;
    else if (strcmp(value, "1") == 0)
        return 1;
    else if (strcmp(value, "0") == 0)
        return 0;

    return -1;
}

int kernel_cmdline_get_bool(const char *name, int def)
{
    struct cmd_arg *arg = find_arg(name);

    if (!arg)
        return def;

    int val = parse_bool(arg->value);
    if (val == -1)  {
        kp(KP_WARNING, "Bool value for arg \"%s\" is invalid. Value: \"%s\". using default: %d\n", arg->name, arg->value, def);
        return def;
    }

    return val;
}

const char *kernel_cmdline_get_string(const char *name, const char *def)
{
    struct cmd_arg *arg = find_arg(name);

    if (arg)
        return arg->value;

    return def;
}
