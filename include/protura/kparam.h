/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_KPARAM_H
#define INCLUDE_PROTURA_KPARAM_H

#include <protura/types.h>
#include <protura/stddef.h>

enum {
    KPARAM_BOOL,
    KPARAM_INT,
    KPARAM_STRING,
    KPARAM_LOGLEVEL,
};

struct kparam {
    const char *name;
    void *param;
    size_t param_size;
    void (*setup) (struct kparam *);
    int type;
};

#define __KPARAM_CHECK(lval, typ) \
    _Generic((lval), \
        int *: (typ) == KPARAM_INT \
            || (typ) == KPARAM_BOOL \
            || (typ) == KPARAM_LOGLEVEL, \
        const char **: (typ) == KPARAM_STRING, \
        default: 0)

#define KPARAM(nam, lval, typ) \
    STATIC_ASSERT(__KPARAM_CHECK(lval, typ)); \
    static const __used __section(".kparam") struct kparam TP(kparam_entry_, __COUNTER__) = \
        { \
            .name = (nam), \
            .param = (lval), \
            .param_size = sizeof((lval)), \
            .type = (typ), \
        }

#define KPARAM_SETUP(nam, lval, typ, set) \
    STATIC_ASSERT(__KPARAM_CHECK(lval, typ)); \
    static const __used __section(".kparam") struct kparam TP(kparam_entry_, __COUNTER__) = \
        { \
            .name = (nam), \
            .param = (lval), \
            .param_size = sizeof((lval)), \
            .type = (typ), \
            .setup = (set), \
        }

void kernel_cmdline_init(void);

#endif
