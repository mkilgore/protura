/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_MM_VM_AREA_H
#define INCLUDE_PROTURA_MM_VM_AREA_H

#include <protura/types.h>
#include <protura/bits.h>
#include <protura/list.h>

enum vm_area_flags {
    VM_AREA_FREE
};

struct vm_area {
    list_node_t vm_area_entry;
    flags_t flags;

    va_t area;
    int page_count;
};

#define VM_AREA_INIT(vm) \
    { \
        .vm_area_entry = LIST_NODE_INIT((vm).vm_area_entry), \
    }

static inline void vm_area_init(struct vm_area *vm)
{
    *vm = (struct vm_area)VM_AREA_INIT(*vm);
}

struct vm_area *vm_area_alloc(int pages);
void vm_area_free(struct vm_area *);

void vm_area_map(va_t va, pa_t address, flags_t vm_flags);
void vm_area_unmap(va_t va);

void vm_area_allocator_init(void);

#endif
