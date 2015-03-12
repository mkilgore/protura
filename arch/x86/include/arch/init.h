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

extern int kernel_is_booting;
extern char kernel_cmdline[];

void cmain(void *kern_start, void *kern_end, uint32_t magic, struct multiboot_info *info);

#endif
