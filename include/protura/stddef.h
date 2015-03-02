/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_STDDEF_H
#define INCLUDE_PROTURA_STDDEF_H

#define NULL ((void *)0)

#define offsetof(s, m) ((size_t)&(((s *)0)->m))

#define alignof(t) __alignof__(t)

#define container_of(ptr, type, member) ({ \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); })

#define TP2(x, y) x ## y
#define TP(x, y) TP2(x, y)

#endif
