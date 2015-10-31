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

static char output[1024];

static int load_bin_elf(struct exe_params *params, struct irq_frame *frame)
{
    struct elf_header head;
    struct elf_prog_section sect;
    int ret, i;
    off_t current_off;
    struct address_space *new_addrspc;

    kprintf("Attempting load of ELF exe...\n");

    ret = vfs_read(params->exe, &head, sizeof(head));
    if (ret != sizeof(head))
        return ret;

    kprintf("ELF header:\n");
    dump_mem(output, sizeof(output), &head, sizeof(head), 0);

    kprintf("%s\n", output);

    if (head.magic != ELF_MAGIC)
        return -ENOEXEC;

    kprintf("Allocing new address_space\n");
    new_addrspc = kmalloc(sizeof(*new_addrspc), PAL_KERNEL);
    address_space_init(new_addrspc);
    kprintf("address_space initalized\n");


    kprintf("Prog section count: %d\n", head.prog_head_count);
    kprintf("Prog head pos: %d\n", head.prog_head_pos);

    for (i = 0, current_off = head.prog_head_pos; i < head.prog_head_count; i++, current_off += sizeof(struct elf_prog_section)) {

        kprintf("Seeking too %d\n", current_off);
        vfs_lseek(params->exe, current_off, SEEK_SET);

        kprintf("Reading header\n");
        ret = vfs_read(params->exe, &sect, sizeof(sect));
        if (ret != sizeof(sect))
            return ret;

        kprintf("Checking header type\n");
        if (sect.type != ELF_PROG_TYPE_LOAD)
            continue;

        kprintf("Allocing new vm_map\n");
        struct vm_map *new_sect = kmalloc(sizeof(struct vm_map), PAL_KERNEL);
        vm_map_init(new_sect);

        kprintf("Setting up vm_map\n");
        new_sect->addr.start = va_make(sect.vaddr);
        new_sect->addr.end = va_make(sect.vaddr + sect.mem_size);

        if (sect.flags & ELF_PROG_FLAG_EXEC)
            flag_set(&new_sect->flags, VM_MAP_EXE);

        if (sect.flags & ELF_PROG_FLAG_READ)
            flag_set(&new_sect->flags, VM_MAP_READ);

        if (sect.flags & ELF_PROG_FLAG_WRITE)
            flag_set(&new_sect->flags, VM_MAP_WRITE);

        kprintf("Map from %p to %p\n", new_sect->addr.start, new_sect->addr.end);

        if (!new_addrspc->code)
            new_addrspc->code = new_sect;
        else if (new_addrspc->code->addr.start > new_sect->addr.start)
            new_addrspc->code = new_sect;

        if (!new_addrspc->data)
            new_addrspc->data = new_sect;
        else if (new_addrspc->data->addr.start < new_sect->addr.start)
            new_addrspc->data = new_sect;

        int pages = PG_ALIGN(sect.mem_size) / PG_SIZE;
        int k;

        kprintf("Segment has %d pages\n", pages);

        for (k = 0; k < pages; k++) {
            off_t len;
            struct page *p = page_get_from_pa(palloc_phys(PAL_KERNEL));

            memset(p->virt, 0, PG_SIZE);

            if (PG_SIZE * k + PG_SIZE < sect.f_size)
                len = PG_SIZE;
            else if (sect.f_size > PG_SIZE * k)
                len = sect.f_size - PG_SIZE * k;
            else
                len = 0;

            kprintf("Len: %d\n", len);

            if (len > 0) {
                vfs_lseek(params->exe, sect.f_off + k * PG_SIZE, SEEK_SET);
                vfs_read(params->exe, p->virt, len);
            }
            list_add(&new_sect->page_list, &p->page_list_node);
        }

        kprintf("Adding to address_space\n");

        address_space_add_vm_map(new_addrspc, new_sect);
    }

    struct vm_map *stack = kmalloc(sizeof(struct vm_map), PAL_KERNEL);
    vm_map_init(stack);

    stack->addr.end = KMEM_PROG_STACK_END;
    stack->addr.start = KMEM_PROG_STACK_START;

    flag_set(&stack->flags, VM_MAP_READ);
    flag_set(&stack->flags, VM_MAP_WRITE);

    int stack_pages = KMEM_STACK_LIMIT;

    struct page *physical_stack_pages = palloc_pages(stack_pages, PAL_KERNEL);

    kprintf("Stack physical pages: %p\n", physical_stack_pages);

    list_attach(&stack->page_list, &physical_stack_pages->page_list_node);
    kprintf("Attacked stack pages to page list\n");

    address_space_add_vm_map(new_addrspc, stack);

    new_addrspc->stack = stack;

    kprintf("New code segment: %p-%p\n", new_addrspc->code->addr.start, new_addrspc->code->addr.end);
    kprintf("New data segment: %p-%p\n", new_addrspc->data->addr.start, new_addrspc->data->addr.end);
    kprintf("New stack segment: %p-%p\n", new_addrspc->stack->addr.start, new_addrspc->stack->addr.end);

    arch_task_change_address_space(new_addrspc);

    frame->esp = (uintptr_t)new_addrspc->stack->addr.end;
    frame->eip = head.entry_vaddr;
    frame->edx = 20;

    frame->cs = _USER_CS | DPL_USER;

    frame->ss = _USER_DS | DPL_USER;
    frame->ds = _USER_DS | DPL_USER;

    frame->eflags = EFLAGS_IF;

    frame->ecx = 20;

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

