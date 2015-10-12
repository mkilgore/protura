/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/scheduler.h>
#include <mm/palloc.h>
#include <arch/idt.h>
#include <drivers/term.h>
#include <arch/task.h>
#include <arch/syscall.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <arch/drivers/pic8259_timer.h>

static void sys_handler_putchar(struct idt_frame *frame)
{
    sys_putchar(frame->ebx);
}

static void sys_handler_clock(struct idt_frame *frame)
{
    frame->eax = sys_clock();
}

static void sys_handler_getpid(struct idt_frame *frame)
{
    frame->eax = sys_getpid();
}

static void sys_handler_putint(struct idt_frame *frame)
{
    sys_putint(frame->ebx);
}

static void sys_handler_putstr(struct idt_frame *frame)
{
    sys_putstr((char *)frame->ebx);
}

static void sys_handler_sleep(struct idt_frame *frame)
{
    sys_sleep(frame->ebx);
}

static void sys_handler_fork(struct idt_frame *frame)
{
    frame->eax = sys_fork();
}

static void sys_handler_getppid(struct idt_frame *frame)
{
    frame->eax = sys_getppid();
}

static struct syscall_handler {
    int num;
    void (*handler) (struct idt_frame *);
} syscall_handlers[] = {
    { SYSCALL_PUTCHAR, sys_handler_putchar },
    { SYSCALL_CLOCK, sys_handler_clock },
    { SYSCALL_GETPID, sys_handler_getpid },
    { SYSCALL_PUTINT, sys_handler_putint },
    { SYSCALL_PUTSTR, sys_handler_putstr },
    { SYSCALL_SLEEP, sys_handler_sleep },
    { SYSCALL_FORK, sys_handler_fork },
    { SYSCALL_GETPPID, sys_handler_getppid },
};

static void syscall_handler(struct idt_frame *frame)
{
    (syscall_handlers[frame->eax].handler) (frame);
}

void syscall_init(void)
{
    irq_register_callback(INT_SYSCALL, syscall_handler, "syscall", IRQ_SYSCALL);
}

