/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/atomic.h>
#include <protura/scheduler.h>
#include <protura/mm/memlayout.h>
#include <protura/drivers/term.h>
#include <protura/snprintf.h>

#include "irq_handler.h"
#include <arch/asm.h>
#include <arch/syscall.h>
#include <arch/drivers/pic8259.h>
#include <arch/gdt.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <arch/idt.h>

static struct idt_ptr idt_ptr;
static struct idt_entry idt_entries[256] = { {0} };

struct idt_identifier {
    atomic32_t count;
    const char *name;
    enum irq_type type;
    void (*handler)(struct irq_frame *);
};

static struct idt_identifier idt_ids[256] = { { ATOMIC32_INIT(0), 0 } };

void irq_register_callback(uint8_t irqno, void (*handler)(struct irq_frame *), const char *id, enum irq_type type)
{
    struct idt_identifier *ident = idt_ids + irqno;

    ident->handler = handler;
    ident->name = id;
    ident->type = type;
}

static const char *cpu_exception_name[] = {
    "Divide by zero",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid OP",
    "Device Not Available",
    "Double Fault",
    "",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "",
    "Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "",
    "Security Exception",
    "",
    "Triple Fault",
    "",
};

void unhandled_cpu_exception(struct irq_frame *frame)
{
    struct task *current = cpu_get_local()->current;

    kp_output_register(term_printfv, "Terminal");

    kp(KP_ERROR, "Exception: %s(%d)! AT: %p, ERR: 0x%08x\n", cpu_exception_name[frame->intno], frame->intno, (void *)frame->eip, frame->err);

    kp(KP_ERROR, "EAX: 0x%08x EBX: 0x%08x\n",
        frame->eax,
        frame->ebx);

    kp(KP_ERROR, "ECX: 0x%08x EDX: 0x%08x\n",
        frame->ecx,
        frame->edx);

    kp(KP_ERROR, "ESI: 0x%08x EDI: 0x%08x\n",
        frame->esi,
        frame->edi);

    kp(KP_ERROR, "ESP: 0x%08x EBP: 0x%08x\n",
        frame->esp,
        frame->ebp);

    kp(KP_ERROR, "Stack backtrace:\n");
    dump_stack_ptr((void *)frame->ebp);
    if (current && !current->kernel) {
        kp(KP_ERROR, "Current running program: %s\n", current->name);
        kp(KP_ERROR, "EAX: 0x%08x EBX: 0x%08x\n",
            current->context.frame->eax,
            current->context.frame->ebx);

        kp(KP_ERROR, "ECX: 0x%08x EDX: 0x%08x\n",
            current->context.frame->ecx,
            current->context.frame->edx);

        kp(KP_ERROR, "ESI: 0x%08x EDI: 0x%08x\n",
            current->context.frame->esi,
            current->context.frame->edi);

        kp(KP_ERROR, "ESP: 0x%08x EBP: 0x%08x\n",
            current->context.frame->esp,
            current->context.frame->ebp);

        kp(KP_ERROR, "User stack dump:\n");
        dump_stack_ptr((void *)current->context.frame->ebp);
    }
    kp(KP_ERROR, "End of backtrace\n");
    kp(KP_ERROR, "Kernel halting\n");

    while (1)
        hlt();
}

void idt_init(void)
{
    int i;

    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uintptr_t)&idt_entries;

    for (i = 0; i < 256; i++)
        IDT_SET_ENT(idt_entries[i], 0, _KERNEL_CS, (uint32_t)(irq_hands[i]), DPL_KERNEL);

    IDT_SET_ENT(idt_entries[INT_SYSCALL], 1, _KERNEL_CS, (uint32_t)(irq_hands[INT_SYSCALL]), DPL_USER);

    for (i = 0; i < 0x20; i++) {
        idt_ids[i].handler = unhandled_cpu_exception;
        idt_ids[i].name = "Unhandled CPU Exception";
        idt_ids[i].type = IRQ_INTERRUPT;
    }

    idt_flush(((uintptr_t)&idt_ptr));
}

void interrupt_dump_stats(void (*print) (const char *fmt, ...))
{
    int k;
    (print) ("Interrupt stats:\n");
    for (k = 0; k < 256; k++) {
        /* Only display IRQ's that have a handler */
        if (!idt_ids[k].handler)
            continue;

        (print) (" %s(%02d) - %d\n", idt_ids[k].name, k, atomic32_get(&idt_ids[k].count));
    }
}

int interrupt_stats_read(void *p, size_t size, size_t *len)
{
    int k;

    *len = 0;

    *len = snprintf(p, size, "Interrupt stats:\n");

    for (k = 0; k < 256 && *len < size; k++) {
        if (!idt_ids[k].handler)
            continue;

        *len += snprintf(p + *len, size - *len, "0x%02x - %d\n", k, atomic32_get(&idt_ids[k].count));
    }

    return 0;
}

void irq_global_handler(struct irq_frame *iframe)
{
    struct idt_identifier *ident = idt_ids + iframe->intno;
    struct task *t;
    struct cpu_info *cpu = cpu_get_local();
    int frame_flag = 0;

    atomic32_inc(&ident->count);

    /* Only actual INTERRUPT types increment the intr_count */
    if (ident->type == IRQ_INTERRUPT)
        cpu->intr_count++;

    /* Check the DPL in the CS from where we came from. If it's the user's DPL,
     * then we just came from user-space */
    t = cpu->current;
    if ((iframe->cs & 0x03) == DPL_USER && t) {
        frame_flag = 1;
        t->context.prev_syscall = iframe->eax;
        t->context.frame = iframe;
    }

    if (iframe->intno >= 0x20 && iframe->intno <= 0x31) {
        if (iframe->intno >= 40)
            outb(PIC8259_IO_PIC2, PIC8259_EOI);
        outb(PIC8259_IO_PIC1, PIC8259_EOI);
    }

    if (ident->handler != NULL)
        (ident->handler) (iframe);

    if (frame_flag && t && t->sig_pending) {
        kp(KP_TRACE, "%d: sig_pending: 0x%08x\n", t->pid, t->sig_pending);
        signal_handle(t, iframe);
    }

    /* There's a possibility that interrupts are on, if this was a syscall.
     *
     * This point represents the end of the interrupt. From here we do clean-up
     * and exit to the running task */
    cli();

    /* If this flag is set, then we're at the end of an interrupt chain - unset
     * 'frame' so that the next interrupt from this task will reconize it's the
     * new first interrupt. Also note. */
    if (frame_flag)
        t->context.frame = NULL;

    /* If this isn't an interrupt irq, then we don't do have to do the
     * stuff after this. */
    if (ident->type == IRQ_INTERRUPT)
        cpu->intr_count--;

    /* Did we die? */
    if (t->killed)
        sys_exit(0);

    /* If something set the reschedule flag and we're the last interrupt
     * (Meaning, we weren't fired while some other interrupt was going on, but
     * when a task was running), then we yield the current task, which 
     * reschedules a new task to start running.
     */
    if (cpu->intr_count == 0 && cpu->reschedule) {
        scheduler_task_yield_preempt();
        cpu->reschedule = 0;
    }

    /* Is he dead yet? */
    if (t->killed)
        sys_exit(0);
}

