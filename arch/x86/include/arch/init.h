/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_INIT_H
#define INCLUDE_ARCH_INIT_H

#include <protura/types.h>
#include <protura/multiboot.h>
#include <protura/kmain.h>

extern char kernel_cmdline[];

void cmain(void *kern_start, void *kern_end, uint32_t magic, struct multiboot_info *info);

extern struct sys_init arch_init_systems[];

#endif
