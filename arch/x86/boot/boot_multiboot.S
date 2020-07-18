/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/multiboot.h>
#include <protura/multiboot2.h>
#include <arch/paging.h>
#include <protura/mm/memlayout.h>
#include <arch/gdt.h>

.section .multiboot

.align 8
multiboot2_header:
.long MULTIBOOT2_HEADER_MAGIC
.long MULTIBOOT2_ARCHITECTURE_I386
.long multiboot2_header_end - multiboot2_header
.long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCHITECTURE_I386 + (multiboot2_header_end - multiboot2_header))

/* Ask for relevant tags to be given to us */
.align 8
required_tags_start:
.short MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST
.short 0
.long required_tags_end - required_tags_start
.long MULTIBOOT2_TAG_TYPE_CMDLINE
.long MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO
.long MULTIBOOT2_TAG_TYPE_MMAP
required_tags_end:

/* Ask for framebuffer */
.align 8
framebuffer_tag_start:
.short MULTIBOOT2_HEADER_TAG_FRAMEBUFFER
.short MULTIBOOT2_HEADER_TAG_OPTIONAL
.long framebuffer_tag_end - framebuffer_tag_start
.long 1280
.long 800
.long 32
framebuffer_tag_end:

/* Mark end of tags */
.align 8
.short MULTIBOOT2_HEADER_TAG_END
.short 0
.long 8

multiboot2_header_end:

multiboot:
.align 8
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_PROTURA_FLAGS
.long MULTIBOOT_PROTURA_CHECKSUM

.section .bootstrap_stack, "aw", @nobits
.align 4
.globl stack_top
stack_botton:
.skip 16384
stack_top:

.section .data

.align 0x1000
initial_pgdir:
.fill 1024, 4, 0

# These store the location of our identity map of MB0
# as well as the high-mem map for the kernel
.set pg0dir, initial_pgdir
.set pg1dir, initial_pgdir + KMEM_KPAGE * 4

.extern pg0
.extern pg1
.extern pg2
.extern pg3

# Temporary GDT with code and data segments for us to use
temp_gdt:
    GDT_SEG_NULL_ASM()
    GDT_SEG_ASM(GDT_TYPE_EXECUTABLE | GDT_TYPE_READABLE, 0x0, 0xFFFFFFFF)
    GDT_SEG_ASM(GDT_TYPE_WRITABLE, 0x0, 0xFFFFFFFF)

temp_gdt_ptr:
    .word (temp_gdt_ptr - temp_gdt - 1)
    .long temp_gdt

start = V2P_WO(_start)

.section .text
.global start
.global _start
.type start, @function
.extern cmain
.extern kern_end

_start:
    movl $V2P_WO(stack_top), %esp

    push %ebx
    push %eax

    # Load our temporary GDT - When we do our jmpl to the higher-half, it won't
    # work without proper segments setup
    lgdt V2P_WO(temp_gdt_ptr)

    # Take the address of pg0, store it in %eax
    # And set the present and writable bits on it
    # in preperation for using it as a page-directory entry
    movl $V2P_WO(pg0), %eax
    orl $(PTE_PRESENT | PTE_WRITABLE), %eax

    movl $V2P_WO(pg0dir), %ebx
    movl $V2P_WO(pg1dir), %edx

    movl $4, %ecx

.pg_dir_loop:
    movl %eax, (%ebx)
    movl %eax, (%edx)
    addl $0x1000, %eax
    addl $4, %ebx
    addl $4, %edx
    loop .pg_dir_loop

    # Put that page table pointer into both of the page directory
    # entries we care about
    # movl %eax, V2P_WO(pg0dir)
    # movl %eax, V2P_WO(pg1dir)

    movl $V2P_WO(initial_pgdir), %eax
    movl %eax, %cr3

    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    jmpl $0x8, $higher_half
higher_half:

    # At this point, the kernel is executing in the 'higher-half'
    # with the code being run at KMEM_KBASE
    addl $KMEM_KBASE, %esp

    push $kern_end
    push $multiboot

    # Clear %ebp before the call - backtrace follows the frame-pointers, so a
    # frame-pointer with an %ebp of zero will mark the end of the backtrace.
    movl $0, %ebp

    call cmain

    addl $0x16, %esp

    cli
.loop:
    hlt
    jmp .loop

