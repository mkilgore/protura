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

#define isr(n) \
.globl isr ## n; isr ## n: pushl $0; pushl $n; jmp idt_handler

#define isre(n) \
.globl isr ## n; isr ## n: pushl $n; jmp idt_handler

isr(0);
isr(1);
isr(2);
isr(3);
isr(4);
isr(5);
isr(6);
isr(7);
isre(8);
isr(9);
isre(10);
isre(11);
isre(12);
isre(13);
isre(14);
isr(15);
isr(16);
isre(17);
isr(18);
isr(19);
isr(20);
isr(21);
isr(22);
isr(23);
isr(24);
isr(25);
isr(26);
isr(27);
isr(28);
isr(29);
isr(30);
isr(31);

/* Regs are at 32(%esp)
 * ISR num is at 36(%esp)
 * err code is at 40(%esp)
 */
idt_handler:
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
    call isr_handler
    addl $4, %esp

    popal
    popl %gs
    popl %fs
    popl %es
    popl %ds
    addl $0x8, %esp # isr num and err code
    iret

