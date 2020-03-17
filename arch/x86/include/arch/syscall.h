/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_SYSCALL_H
#define INCLUDE_ARCH_SYSCALL_H

#include <uapi/arch/syscall.h>

#ifndef ASM
#include <protura/irq.h>

void syscall_init(void);

#endif

#endif
