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

    struct vm_map *code, *data, *stack;
};

/* A description of a single contiguous map of physical pages to virtual
 * pages in a task's address space. The virtual pages are contiguous, the
 * physical ones need not be.
 *
 * Note that 'addr' need not be page-aligned, though obviously the mapping is
 * done on page-sized chunks. The list of physical pages to map is 'page_list'.
  */
struct vm_map {
    struct address_space *owner;
    list_node_t address_space_entry;

    struct vm_region addr;

    flags_t flags;

    list_head_t page_list;
};

enum vm_map_flags {
    VM_MAP_READ,
    VM_MAP_WRITE,
    VM_MAP_EXE,
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
        .page_list = LIST_HEAD_INIT((vm_map).page_list), \
    }

static inline void address_space_init(struct address_space *addrspc)
{
    *addrspc = (struct address_space)ADDRESS_SPACE_INIT(*addrspc);
    arch_address_space_init(addrspc);
}

static inline void address_space_clear(struct address_space *addrspc)
{
    arch_address_space_clear(addrspc);
}

static inline void vm_map_init(struct vm_map *map)
{
    *map = (struct vm_map)VM_MAP_INIT(*map);
}

void address_space_map_vm_entry(struct address_space *, va_t virtual, pa_t physical, flags_t vm_flags);
void address_space_map_vm_map(struct address_space *, struct vm_map *map);

void address_space_unmap_vm_entry(struct address_space *, va_t virtual);
void address_space_unmap_vm_map(struct address_space *, struct vm_map *map);

/* Resize a vm_map.
 *
 * Note that if this vm_map is owned by an address_space, then it will also
 * map/unmap the new space into that address_space. */
void vm_map_resize(struct vm_map *map, struct vm_region new_size);

/* Adds a 'vm_map' to this address_space's list of vm_map's, and maps any of
 * it's current pages */
void address_space_vm_map_add(struct address_space *, struct vm_map *map);
void address_space_vm_map_remove(struct address_space *, struct vm_map *map);

void address_space_copy(struct address_space *new, struct address_space *old);

#endif
