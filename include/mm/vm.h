/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_VM_H
#define INCLUDE_MM_VM_H

#include <protura/types.h>
#include <protura/bits.h>

struct inode;
struct page;

struct vm_area {
    va_t start_addr;
    va_t end_addr;

    unsigned int perms;

    struct inode *inode;
    pn_t inode_off; /* Offset into inode, in pages */
};

struct vm_area_ops {
    struct page *(*getpage) (struct vm_area *, va_t addr);
};

enum vm_perms {
    VM_READ,
    VM_WRITE,
    VM_EXE,
};

#define vm_area_is_readable(area)   test_bit(&area->perms, VM_READ)
#define vm_area_is_writeable(area)  test_bit(&area->perms, VM_WRITE)
#define vm_area_is_executable(area) test_bit(&area->perms, VM_EXE)

#endif
