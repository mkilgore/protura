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
#include <mm/kmalloc.h>
#include <arch/task.h>
#include <arch/paging.h>
#include <arch/idt.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/file.h>
#include <fs/stat.h>
#include <fs/inode.h>
#include <fs/namei.h>
#include <fs/sys.h>
#include <fs/vfs.h>
#include <fs/binfmt.h>
#include <fs/exec.h>
#include <fs/elf.h>

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
        return ret;

    if (head.magic != ELF_MAGIC)
        return -ENOEXEC;

    new_addrspc = kmalloc(sizeof(*new_addrspc), PAL_KERNEL);
    address_space_init(new_addrspc);

    kp(KP_TRACE, "Parsing ELF binary... frame: %p, state: %d\n", frame, cpu_get_local()->current->state);

    /* The idea is that you loop over every header, and if a header's type is
     * 'LOAD' then we make a vm_map for it and load it into memory. */
    for (i = 0, current_off = head.prog_head_pos; i < head.prog_head_count; i++, current_off += sizeof(struct elf_prog_section)) {

        kp(KP_TRACE, "Reading ELF section...\n");
        vfs_lseek(params->exe, current_off, SEEK_SET);
        ret = vfs_read(params->exe, &sect, sizeof(sect));
        kp(KP_TRACE, "Reading ret: %d\n", ret);
        if (ret != sizeof(sect))
            return ret;

        if (sect.type != ELF_PROG_TYPE_LOAD)
            continue;

        kp(KP_TRACE, "Creating new vm_map...\n");
        struct vm_map *new_sect = kmalloc(sizeof(struct vm_map), PAL_KERNEL);
        vm_map_init(new_sect);

        new_sect->addr.start = va_make(sect.vaddr);
        new_sect->addr.end = va_make(sect.vaddr + sect.mem_size);

        if (sect.flags & ELF_PROG_FLAG_EXEC)
            flag_set(&new_sect->flags, VM_MAP_EXE);

        if (sect.flags & ELF_PROG_FLAG_READ)
            flag_set(&new_sect->flags, VM_MAP_READ);

        if (sect.flags & ELF_PROG_FLAG_WRITE)
            flag_set(&new_sect->flags, VM_MAP_WRITE);

        kp(KP_TRACE, "Map from %p to %p\n", new_sect->addr.start, new_sect->addr.end);

        if (!new_addrspc->code)
            new_addrspc->code = new_sect;
        else if (new_addrspc->code->addr.start > new_sect->addr.start)
            new_addrspc->code = new_sect;

        if (!new_addrspc->data)
            new_addrspc->data = new_sect;
        else if (new_addrspc->data->addr.start < new_sect->addr.start)
            new_addrspc->data = new_sect;

        /* f_size is the size in the file, mem_size is the size in memory of
         * our copy.
         *
         * If mem_size > f_size, then the empty space is filled with zeros. */
        int pages = PG_ALIGN(sect.mem_size) / PG_SIZE;
        int k;

        for (k = 0; k < pages; k++) {
            off_t len;
            struct page *p = palloc(0, PAL_KERNEL);

            if (PG_SIZE * k + PG_SIZE < sect.f_size)
                len = PG_SIZE;
            else if (sect.f_size > PG_SIZE * k)
                len = sect.f_size - PG_SIZE * k;
            else
                len = 0;

            if (len > 0) {
                vfs_lseek(params->exe, sect.f_off + k * PG_SIZE, SEEK_SET);
                vfs_read(params->exe, p->virt, len);
            }

            if (len < PG_SIZE)
                memset(p->virt + len, 0, PG_SIZE - len);

            kp(KP_TRACE, "Page: %p\n", p);
            list_add_tail(&new_sect->page_list, &p->page_list_node);
        }

        address_space_vm_map_add(new_addrspc, new_sect);
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

    flag_set(&stack->flags, VM_MAP_READ);
    flag_set(&stack->flags, VM_MAP_WRITE);

    palloc_unordered(&stack->page_list, KMEM_STACK_LIMIT, PAL_KERNEL);

    address_space_vm_map_add(new_addrspc, stack);

    new_addrspc->stack = stack;

    kp(KP_TRACE, "New code segment: %p-%p\n", new_addrspc->code->addr.start, new_addrspc->code->addr.end);
    if (new_addrspc->data)
        kp(KP_TRACE, "New data segment: %p-%p\n", new_addrspc->data->addr.start, new_addrspc->data->addr.end);
    kp(KP_TRACE, "New stack segment: %p-%p\n", new_addrspc->stack->addr.start, new_addrspc->stack->addr.end);

    current = cpu_get_local()->current;

    arch_task_change_address_space(new_addrspc);

    irq_frame_initalize(current->context.frame);

    irq_frame_set_stack(current->context.frame, new_addrspc->stack->addr.end);
    irq_frame_set_ip(current->context.frame, va_make(head.entry_vaddr));

    return 0;
}

static struct binfmt binfmt_elf = BINFMT_INIT(binfmt_elf, "elf", load_bin_elf);

void elf_register(void)
{
    binfmt_register(&binfmt_elf);
}

void elf_unregister(void)
{
    binfmt_unregister(&binfmt_elf);
}

