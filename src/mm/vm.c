/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/snprintf.h>
#include <mm/kmalloc.h>
#include <mm/memlayout.h>
#include <mm/vm.h>


void address_space_map_vm_entry(struct address_space *addrspc, va_t virtual, pa_t physical, flags_t vm_flags)
{
    arch_address_space_map_vm_entry(addrspc, virtual, physical, vm_flags);
}

void address_space_map_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    arch_address_space_map_vm_map(addrspc, map);
}

void address_space_unmap_vm_entry(struct address_space *addrspc, va_t virtual)
{
    arch_address_space_unmap_vm_map(addrspc, virtual);
}

void address_space_unmap_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    arch_address_space_unmap_vm_map(addrspc, map);
}

void address_space_copy(struct address_space *new, struct address_space *old)
{
    arch_address_space_copy(new, old);
}

void address_space_add_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    list_add(&addrspc->vm_maps, &map->address_space_entry);
    address_space_map_vm_map(addrspc, map);
}

