/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_PAGING_H
#define INCLUDE_ARCH_PAGING_H

#include <protura/types.h>
#include <protura/multiboot.h>

void paging_init(void *kernel_end, struct multiboot_info *info);

#endif
