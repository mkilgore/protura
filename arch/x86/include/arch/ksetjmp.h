/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_KSETJMP_H
#define INCLUDE_ARCH_KSETJMP_H

#include <protura/types.h>
#include <protura/compiler.h>

struct x86_jmpbuf {
    uint32_t ebx, esi, edi, ebp, esp, return_address;
} __packed;

typedef struct x86_jmpbuf kjmpbuf_t;

int ksetjmp(kjmpbuf_t *buf) __attribute__((returns_twice));
int klongjmp(kjmpbuf_t *buf, int ret_value);

#endif
