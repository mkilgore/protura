/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <arch/gdt.h>

.globl idt_flush
idt_flush:
    movl 4(%esp), %eax
    lidt (%eax)
    ret

 .globl irq_handler
irq_handler:
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    movw $_KERNEL_DS, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %esp
    call irq_global_handler
    addl $4, %esp

    popal
    popl %gs
    popl %fs
    popl %es
    popl %ds
    addl $0x8, %esp # irq num and err code
    iretl

