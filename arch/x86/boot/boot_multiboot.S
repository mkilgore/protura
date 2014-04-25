/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/multiboot.h>
#include <mm/memlayout.h>

.section .multiboot
multiboot:
.align 4
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

# For identify map of 0M to end of kernel (Map of 1M stays)
pg0:
x = 0
.rept 0x400
.long x | 0x1 | 0x2
x = x + 1
.endr

# For map of kernel to proper KMEM_KBASE location
pg1: 
x = 0
.rept 0x400
.long (x + KMEM_KPAGE) | 0x1 | 0x2
x = x + 1
.endr

pd: # Initial page directory
#pg0loc: .long 0x1 | 0x2
.long 0x83
.skip (KMEM_KPAGE - 1) * 4
#pg1loc: .long 0x1 | 0x2
.long 0x83
.skip (1024 - KMEM_KPAGE - 1) * 4

start = _start - KMEM_KBASE

.section .text
.global start
.type start, @function
.extern cmain
.extern kern_end

_start:
    movl $stack_top - KMEM_KBASE, %esp

    push %ebx
    push %eax

    movl %cr4, %eax
    orl $0x10, %eax
    movl %eax, %cr4

    movl $(pd - KMEM_KBASE), %eax
    movl %eax, %cr3

    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    jmpl $0x8, $higher_half

higher_half:

    addl $KMEM_KBASE, %esp

    push $kern_end
    push $multiboot

    call cmain

    addl $0x16, %esp

    cli
    hlt
.loop:
    jmp .loop

