/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/multiboot.h>

.section .multiboot
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_PROTURA_FLAGS
.long MULTIBOOT_PROTURA_CHECKSUM

.section .bootstrap_stack, "aw", @nobits
.align 4
stack_botton:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
.extern cmain
_start:

    cli
    movl $stack_top, %esp

    push %ebx
    push %eax

    call cmain

    addl $0x8, %esp

    cli
    hlt
.loop:
    jmp .loop

