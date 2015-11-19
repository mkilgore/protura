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
#include <fs/sys.h>

static void sys_handler_putchar(struct irq_frame *frame)
{
    sys_putchar(frame->ebx);
}

static void sys_handler_clock(struct irq_frame *frame)
{
    frame->eax = sys_clock();
}

static void sys_handler_getpid(struct irq_frame *frame)
{
    frame->eax = sys_getpid();
}

static void sys_handler_putint(struct irq_frame *frame)
{
    sys_putint(frame->ebx);
}

static void sys_handler_putstr(struct irq_frame *frame)
{
    sys_putstr((char *)frame->ebx);
}

static void sys_handler_sleep(struct irq_frame *frame)
{
    sys_sleep(frame->ebx);
}

static void sys_handler_fork(struct irq_frame *frame)
{
    frame->eax = sys_fork();
}

static void sys_handler_getppid(struct irq_frame *frame)
{
    frame->eax = sys_getppid();
}

static void sys_handler_open(struct irq_frame *frame)
{
    frame->eax = sys_open((char *)frame->ebx, frame->ecx, frame->edx);
}

static void sys_handler_close(struct irq_frame *frame)
{
    frame->eax = sys_close(frame->ebx);
}

static void sys_handler_read(struct irq_frame *frame)
{
    frame->eax = sys_read(frame->ebx, (char *)frame->ecx, frame->edx);
}

static void sys_handler_write(struct irq_frame *frame)
{
    frame->eax = sys_write(frame->ebx, (char *)frame->ecx, frame->edx);
}

static void sys_handler_lseek(struct irq_frame *frame)
{
    frame->eax = sys_lseek(frame->ebx, frame->ecx, frame->edx);
}

static void sys_handler_exec(struct irq_frame *frame)
{
    sys_exec((char *)frame->ebx, (char *const *)frame->ecx, frame);
}

static void sys_handler_yield(struct irq_frame *frame)
{
    scheduler_task_yield();
}

static void sys_handler_exit(struct irq_frame *frame)
{
    sys_exit(frame->ebx);
}

static void sys_handler_wait(struct irq_frame *frame)
{
    frame->eax = sys_wait((int *)frame->ebx);
}

static void sys_handler_dup(struct irq_frame *frame)
{
    frame->eax = sys_dup(frame->ebx);
}

static void sys_handler_dup2(struct irq_frame *frame)
{
    frame->eax = sys_dup2(frame->ebx, frame->ecx);
}

#define SYSCALL(call, handler) \
    [SYSCALL_##call] = { SYSCALL_##call, handler }

static struct syscall_handler {
    int num;
    void (*handler) (struct irq_frame *);
} syscall_handlers[] = {
    SYSCALL(PUTCHAR, sys_handler_putchar),
    SYSCALL(CLOCK, sys_handler_clock),
    SYSCALL(GETPID, sys_handler_getpid),
    SYSCALL(PUTINT, sys_handler_putint),
    SYSCALL(PUTSTR, sys_handler_putstr),
    SYSCALL(SLEEP, sys_handler_sleep),
    SYSCALL(FORK, sys_handler_fork),
    SYSCALL(GETPPID, sys_handler_getppid),
    SYSCALL(OPEN, sys_handler_open),
    SYSCALL(CLOSE, sys_handler_close),
    SYSCALL(READ, sys_handler_read),
    SYSCALL(WRITE, sys_handler_write),
    SYSCALL(LSEEK, sys_handler_lseek),
    SYSCALL(EXEC, sys_handler_exec),
    SYSCALL(YIELD, sys_handler_yield),
    SYSCALL(EXIT, sys_handler_exit),
    SYSCALL(WAIT, sys_handler_wait),
    SYSCALL(DUP, sys_handler_dup),
    SYSCALL(DUP2, sys_handler_dup2),
};

static void syscall_handler(struct irq_frame *frame)
{
    (syscall_handlers[frame->eax].handler) (frame);
}

void syscall_init(void)
{
    irq_register_callback(INT_SYSCALL, syscall_handler, "syscall", IRQ_SYSCALL);
}

