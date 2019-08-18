/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>
#include <arch/task.h>
#include <arch/paging.h>
#include <arch/idt.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/namei.h>
#include <protura/fs/sys.h>
#include <protura/fs/vfs.h>
#include <protura/fs/binfmt.h>
#include <protura/fs/elf.h>

#ifdef CONFIG_KERNEL_LOG_ELF
# define kp_elf(params, str, ...) kp(KP_DEBUG, "%p elf: " str, params, ## __VA_ARGS__)
#else
# define kp_elf(params, str, ...) do { ; } while (0)
#endif

static int vm_map_pad_last_page(struct exe_params *params, struct vm_map *map, off_t file_size)
{
    struct page *p = pzalloc(0, PAL_KERNEL);
    if (!p)
        return -ENOSPC;

    va_t address = map->addr.end - PG_SIZE;

    off_t file_offset = PG_ALIGN_DOWN(file_size) + map->file_page_offset;
    int off = file_size - PG_ALIGN_DOWN(file_size);

    vfs_lseek(map->filp, file_offset, SEEK_SET);
    int err = vfs_read(map->filp, p->virt, off);
    kp_elf(params, "read: %d\n", err);
    if (err < 0) {
        pfree(p, 0);
        return err;
    }

    kp_elf(params, "file_size: %ld, align_file_size: %ld, off: 0x%04x\n", file_size, PG_ALIGN_DOWN(file_size), off);
    kp_elf(params, "Mapping last page of map %p-%p: %p\n", map->addr.start, map->addr.end, address);

    page_table_map_entry(map->owner->page_dir, address, page_to_pa(p), map->flags);
    return 0;
}

static int add_data_vm_map(struct exe_params *params, struct address_space *addrspc, struct elf_prog_section *sect)
{
    kp_elf(params, "Creating new data vm_map...\n");
    struct vm_map *data_sect = kmalloc(sizeof(struct vm_map), PAL_KERNEL);
    vm_map_init(data_sect);

    data_sect->owner = addrspc;

    kp_elf(params, "Sect: %p\n", sect);

    /* FIXME: On error this needs to properly free things */

    data_sect->addr.start = va_make(PG_ALIGN_DOWN(sect->vaddr));
    data_sect->addr.end = va_make(PG_ALIGN(sect->vaddr + sect->f_size));

    if (sect->flags & ELF_PROG_FLAG_EXEC)
        flag_set(&data_sect->flags, VM_MAP_EXE);

    if (sect->flags & ELF_PROG_FLAG_READ)
        flag_set(&data_sect->flags, VM_MAP_READ);

    if (sect->flags & ELF_PROG_FLAG_WRITE)
        flag_set(&data_sect->flags, VM_MAP_WRITE);

    kp_elf(params, "Map from %p to %p\n", data_sect->addr.start, data_sect->addr.end);

    if (!addrspc->code)
        addrspc->code = data_sect;
    else if (addrspc->code->addr.start > data_sect->addr.start)
        addrspc->code = data_sect;

    if (!addrspc->data)
        addrspc->data = data_sect;
    else if (addrspc->data->addr.start < data_sect->addr.start)
        addrspc->data = data_sect;

    data_sect->filp = file_dup(params->exe);
    data_sect->file_page_offset = PG_ALIGN_DOWN(sect->f_off);
    data_sect->ops = &mmap_file_ops;
    kp_elf(params, "file_page_offset: %ld\n", data_sect->file_page_offset);

    /* We have to adjust the file size since we aligned the file offset to the previous page */
    off_t adjusted_file_size = sect->f_size + (sect->f_off - data_sect->file_page_offset);

    if (adjusted_file_size != PG_ALIGN(adjusted_file_size)) {
        kp_elf(params, "Padding last page of new_sect, %ld vs %ld\n", adjusted_file_size, PG_ALIGN(adjusted_file_size));
        vm_map_pad_last_page(params, data_sect, adjusted_file_size);
    }

    address_space_vm_map_add(addrspc, data_sect);
    return 0;
}

static int add_bss_vm_map(struct exe_params *params, struct address_space *addrspc, struct elf_prog_section *sect)
{
    va_t start = va_make(PG_ALIGN(sect->vaddr + sect->f_size));
    va_t end = va_make(PG_ALIGN(sect->vaddr + sect->mem_size));
    if (start == end) {
        kp_elf(params, "No bss segment required\n");
        return 0;
    }

    kp_elf(params, "Creating new bss vm_map...\n");
    struct vm_map *bss_sect = kmalloc(sizeof(struct vm_map), PAL_KERNEL);
    vm_map_init(bss_sect);

    bss_sect->owner = addrspc;
    bss_sect->addr.start = start;
    bss_sect->addr.end = end;

    if (sect->flags & ELF_PROG_FLAG_EXEC)
        flag_set(&bss_sect->flags, VM_MAP_EXE);

    if (sect->flags & ELF_PROG_FLAG_READ)
        flag_set(&bss_sect->flags, VM_MAP_READ);

    if (sect->flags & ELF_PROG_FLAG_WRITE)
        flag_set(&bss_sect->flags, VM_MAP_WRITE);

    kp_elf(params, "BSS: Map from %p to %p\n", bss_sect->addr.start, bss_sect->addr.end);

    addrspc->bss = bss_sect;
    addrspc->brk = bss_sect->addr.end;
    address_space_vm_map_add(addrspc, bss_sect);
    return 0;
}

static int load_bin_elf(struct exe_params *params, struct irq_frame *frame)
{
    struct elf_header head;
    struct elf_prog_section sect;
    int ret, i;
    off_t current_off;
    struct address_space *new_addrspc;
    struct task *current;

    ret = vfs_read(params->exe, &head, sizeof(head));
    if (ret != sizeof(head))
        return -ENOEXEC;

    if (head.magic != ELF_MAGIC)
        return -ENOEXEC;

    new_addrspc = kmalloc(sizeof(*new_addrspc), PAL_KERNEL);
    address_space_init(new_addrspc);

    kp_elf(params, "Parsing ELF binary... frame: %p, state: %d\n", frame, cpu_get_local()->current->state);

    /* The idea is that you loop over every header, and if a header's type is
     * 'LOAD' then we make a vm_map for it and load it into memory. */
    for (i = 0, current_off = head.prog_head_pos; i < head.prog_head_count; i++, current_off += sizeof(struct elf_prog_section)) {

        kp_elf(params, "Reading ELF section...\n");
        vfs_lseek(params->exe, current_off, SEEK_SET);
        ret = vfs_read(params->exe, &sect, sizeof(sect));
        kp_elf(params, "Reading ret: %d\n", ret);
        if (ret != sizeof(sect))
            return -ENOEXEC;

        if (sect.type != ELF_PROG_TYPE_LOAD)
            continue;

        ret = add_data_vm_map(params, new_addrspc, &sect);
        if (ret)
            return -ENOEXEC;

        ret = add_bss_vm_map(params, new_addrspc, &sect);
        if (ret)
            return -ENOEXEC;
    }

    /* If we detected both the code and data segments to be the same segment,
     * then that means we don't actually have a data segment, so we set it to
     * NULL. This only actually matters for setting the BRK, and we'll just
     * make a new vm_map if the data is NULL in that case. */
    if (new_addrspc->code == new_addrspc->data)
        new_addrspc->data = NULL;

    struct vm_map *stack = kmalloc(sizeof(struct vm_map), PAL_KERNEL);
    vm_map_init(stack);

    stack->addr.end = KMEM_PROG_STACK_END;
    stack->addr.start = KMEM_PROG_STACK_START;
    stack->owner = new_addrspc;

    flag_set(&stack->flags, VM_MAP_READ);
    flag_set(&stack->flags, VM_MAP_WRITE);
    flag_set(&stack->flags, VM_MAP_EXE);

    address_space_vm_map_add(new_addrspc, stack);

    new_addrspc->stack = stack;

    kp_elf(params, "New code segment: %p-%p\n", new_addrspc->code->addr.start, new_addrspc->code->addr.end);

    if (new_addrspc->data)
        kp_elf(params, "New data segment: %p-%p\n", new_addrspc->data->addr.start, new_addrspc->data->addr.end);

    if (new_addrspc->bss)
        kp_elf(params, "New bss segment: %p-%p\n", new_addrspc->bss->addr.start, new_addrspc->bss->addr.end);

    kp_elf(params, "New stack segment: %p-%p\n", new_addrspc->stack->addr.start, new_addrspc->stack->addr.end);

    current = cpu_get_local()->current;

    address_space_change(new_addrspc);

    irq_frame_initalize(current->context.frame);

    irq_frame_set_stack(current->context.frame, new_addrspc->stack->addr.end);
    irq_frame_set_ip(current->context.frame, va_make(head.entry_vaddr));

    kp_elf(params, "ELF load complete\n");

    return 0;
}

static struct binfmt binfmt_elf = BINFMT_INIT(binfmt_elf, "elf",  "\177ELF", load_bin_elf);

void elf_register(void)
{
    binfmt_register(&binfmt_elf);
}

void elf_unregister(void)
{
    binfmt_unregister(&binfmt_elf);
}

