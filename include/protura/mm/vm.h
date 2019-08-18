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
#include <protura/list.h>
#include <protura/bits.h>
#include <arch/task.h>

#include <arch/paging.h>
#include <arch/ptable.h>

struct inode;
struct page;

struct address_space;
struct vm_map;

/* Memory areas are expressed using a starting and ending address. The
 * convention is to consider the start to be inclusive, and the end to be
 * exclusive. */
struct vm_region {
    va_t start, end;
};

/* A task's mapped address space */
struct address_space {
    list_head_t vm_maps;

    pgd_t *page_dir;

    struct vm_map *code, *data, *bss, *stack;
    va_t brk;
};

struct vm_map_ops {
    int (*fill_page) (struct vm_map *, va_t address);
};

/* A description of a single contiguous map of physical pages to virtual
 * pages in a task's address space. The virtual pages are contiguous, the
 * physical ones need not be. Each map has a unique set of ops for handling
 * page faults.
 *
 * Note that these maps are assumed to be page aligned
  */
struct vm_map {
    struct address_space *owner;
    list_node_t address_space_entry;

    struct vm_region addr;

    flags_t flags;

    struct file *filp;
    off_t file_page_offset;

    const struct vm_map_ops *ops;
};

enum vm_map_flags {
    VM_MAP_READ,
    VM_MAP_WRITE,
    VM_MAP_EXE,
    VM_MAP_NOCACHE,
    VM_MAP_WRITETHROUGH,
};

#define vm_map_is_readable(map)   flag_test(&(map)->flags, VM_MAP_READ)
#define vm_map_is_writeable(map)  flag_test(&(map)->flags, VM_MAP_WRITE)
#define vm_map_is_executable(map) flag_test(&(map)->flags, VM_MAP_EXE)

#define ADDRESS_SPACE_INIT(addrspc) \
    { \
        .vm_maps = LIST_HEAD_INIT((addrspc).vm_maps), \
        .code = NULL, \
        .data = NULL, \
        .stack = NULL, \
    }

#define VM_MAP_INIT(vm_map) \
    { \
        .address_space_entry = LIST_NODE_INIT((vm_map).address_space_entry), \
        .addr = { 0, 0 }, \
        .flags = 0, \
    }

static inline void address_space_init(struct address_space *addrspc)
{
    *addrspc = (struct address_space)ADDRESS_SPACE_INIT(*addrspc);
    addrspc->page_dir = page_table_new();
}

static inline void vm_map_init(struct vm_map *map)
{
    *map = (struct vm_map)VM_MAP_INIT(*map);
}
/* Resize a vm_map.
 *
 * Note that if this vm_map is owned by an address_space, then it will also
 * map/unmap the new space into that address_space. */
void vm_map_resize(struct vm_map *map, struct vm_region new_size);

void address_space_change(struct address_space *new);
void address_space_clear(struct address_space *addrspc);
void address_space_copy(struct address_space *new, struct address_space *old);
void address_space_vm_map_add(struct address_space *, struct vm_map *);
void address_space_vm_map_remove(struct address_space *, struct vm_map *);
int address_space_handle_pagefault(struct address_space *, va_t address);

extern const struct vm_map_ops mmap_file_ops;

void *sys_sbrk(intptr_t increment);
void sys_brk(va_t new_end);

#endif
