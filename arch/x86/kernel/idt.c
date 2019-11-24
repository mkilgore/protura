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
#include <protura/mm/kmalloc.h>
#include <protura/drivers/console.h>
#include <protura/snprintf.h>
#include <protura/fs/seq_file.h>

#include "irq_handler.h"
#include <arch/asm.h>
#include <arch/syscall.h>
#include <arch/drivers/pic8259.h>
#include <arch/cpuid.h>
#include <arch/gdt.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <arch/idt.h>

static struct idt_ptr idt_ptr;
static struct idt_entry idt_entries[256] = { {0} };

struct idt_identifier {
    atomic32_t count;
    enum irq_type type;
    flags_t flags;

    list_head_t list;
};

static struct idt_identifier idt_ids[256];

int x86_register_interrupt_handler(uint8_t irqno, struct irq_handler *hand)
{
    int err = 0;
    irq_flags_t flags;
    int enable = 0;
    struct idt_identifier *ident = idt_ids + irqno;

    /* FIXME: Because we don't yet support SMP, we're avoiding locking the
     * idt_identifier entries using a spinlock, instead we're protecting them
     * by simply ensuring interrupts are always off while manipulating them. */
    flags = irq_save();
    irq_disable();

    if (!list_empty(&ident->list)) {
        if (!flag_test(&ident->flags, IRQF_SHARED)) {
            err = -1;
            goto restore_flags;
        }

        if (ident->type != hand->type) {
            err = -1;
            goto restore_flags;
        }
    } else {
        enable = 1;
        ident->type = hand->type;
        ident->flags = hand->flags;
    }

    kp(KP_NORMAL, "interrupt %d, name: %s\n", irqno, hand->id);

    list_add_tail(&ident->list, &hand->entry);

    if (enable && irqno >= PIC8259_IRQ0 && irqno <= PIC8259_IRQ0 + 16)
        pic8259_enable_irq(irqno - PIC8259_IRQ0);

  restore_flags:
    irq_restore(flags);
    return err;
}

int cpu_exception_register_callback(uint8_t exception_no, struct irq_handler *hand)
{
    return x86_register_interrupt_handler(exception_no, hand);
}

int irq_register_handler(uint8_t irqno, struct irq_handler *hand)
{
    return x86_register_interrupt_handler(irqno + PIC8259_IRQ0, hand);
}

int irq_register_callback(uint8_t irqno, void (*handler)(struct irq_frame *, void *param), const char *id, enum irq_type type, void *param, int flags)
{
    struct irq_handler *hand = kmalloc(sizeof(*hand), PAL_KERNEL);
    irq_handler_init(hand);

    hand->callback = handler;
    hand->type = type;
    hand->id = id;
    hand->param = param;
    hand->flags = flags;

    int err = irq_register_handler(irqno, hand);

    if (err)
        kfree(hand);

    return err;
}

static const char *cpu_exception_name[32] = {
    [0] = "Divide by zero",
    [1] = "Debug",
    [2] = "NMI",
    [3] = "Breakpoint",
    [4] = "Overflow",
    [5] = "Bound Range Exceeded",
    [6] = "Invalid OP",
    [7] = "Device Not Available",
    [8] = "Double Fault",
    [10] = "Invalid TSS",
    [11] = "Segment Not Present",
    [12] = "Stack-Segment Fault",
    [13] = "General Protection Fault",
    [14] = "Page Fault",
    [16] = "Floating-Point Exception",
    [17] = "Alignment Check",
    [18] = "Machine Check",
    [19] = "SIMD Floating-Point Exception",
    [20] = "Virtualization Exception",
    [30] = "Security Exception",
};

void unhandled_cpu_exception(struct irq_frame *frame, void *param)
{
    struct task *current = cpu_get_local()->current;

    vt_console_kp_register();

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

    kp(KP_ERROR, "CS: 0x%04x SS: 0x%04x\n",
        frame->cs,
        frame->ss);

    kp(KP_ERROR, "DS: 0x%04x ES: 0x%04x\n",
        frame->ds,
        frame->es);

    kp(KP_ERROR, "FS: 0x%04x GS: 0x%04x\n",
        frame->fs,
        frame->gs);

    kp(KP_ERROR, "Stack backtrace:\n");
    dump_stack_ptr((void *)frame->ebp);
    if (current && !flag_test(&current->flags, TASK_FLAG_KERNEL)) {
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

static void div_by_zero_handler(struct irq_frame *frame, void *param)
{
    if ((frame->cs & 0x03) != DPL_USER)
        unhandled_cpu_exception(frame, param);

    struct task *current = cpu_get_local()->current;

    scheduler_task_send_signal(current->pid, SIGFPE, 1);
}

static struct irq_handler cpu_exceptions[] = {
    [0] = IRQ_HANDLER_INIT(cpu_exceptions[0], "Divide By Zero", div_by_zero_handler, NULL, IRQ_INTERRUPT, 0),
    [1] = IRQ_HANDLER_INIT(cpu_exceptions[0], "Debug", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [2] = IRQ_HANDLER_INIT(cpu_exceptions[2], "NMI", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [3] = IRQ_HANDLER_INIT(cpu_exceptions[3], "Breakpoint", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [4] = IRQ_HANDLER_INIT(cpu_exceptions[4], "Overflow", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [5] = IRQ_HANDLER_INIT(cpu_exceptions[5], "Bound Range Exceeded", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [6] = IRQ_HANDLER_INIT(cpu_exceptions[6], "Invalid OP", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [7] = IRQ_HANDLER_INIT(cpu_exceptions[7], "Device Not Available", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [8] = IRQ_HANDLER_INIT(cpu_exceptions[8], "Double Fault", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [10] = IRQ_HANDLER_INIT(cpu_exceptions[10], "Invalid TSS", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [11] = IRQ_HANDLER_INIT(cpu_exceptions[11], "Segment Not Present", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [12] = IRQ_HANDLER_INIT(cpu_exceptions[12], "Stack-Segment Fault", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [13] = IRQ_HANDLER_INIT(cpu_exceptions[13], "General Protection Fault", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [16] = IRQ_HANDLER_INIT(cpu_exceptions[16], "Floating-Point Exception", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [17] = IRQ_HANDLER_INIT(cpu_exceptions[17], "Alignment Check", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [18] = IRQ_HANDLER_INIT(cpu_exceptions[18], "Machine Check", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [19] = IRQ_HANDLER_INIT(cpu_exceptions[19], "SIMD Floating-Point Exception", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [20] = IRQ_HANDLER_INIT(cpu_exceptions[20], "Virtualization Exception", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
    [30] = IRQ_HANDLER_INIT(cpu_exceptions[30], "Security Exception", unhandled_cpu_exception, NULL, IRQ_INTERRUPT, 0),
};

void idt_init(void)
{
    int i;

    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uintptr_t)&idt_entries;

    for (i = 0; i < 256; i++) {
        list_head_init(&idt_ids[i].list);

        IDT_SET_ENT(idt_entries[i], 0, _KERNEL_CS, (uint32_t)(irq_hands[i]), DPL_KERNEL);
    }

    IDT_SET_ENT(idt_entries[INT_SYSCALL], 1, _KERNEL_CS, (uint32_t)(irq_hands[INT_SYSCALL]), DPL_USER);

    for (i = 0; i < ARRAY_SIZE(cpu_exceptions); i++)
        if (cpu_exceptions[i].callback)
            cpu_exception_register_callback(i, cpu_exceptions + i);

    idt_flush(((uintptr_t)&idt_ptr));
}

void irq_global_handler(struct irq_frame *iframe)
{
    struct idt_identifier *ident = idt_ids + iframe->intno;
    struct task *t;
    struct cpu_info *cpu = cpu_get_local();
    int frame_flag = 0;
    int pic8259_irq = -1;

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

    if (cpuid_has_sse() && t)
        i387_fxsave(&t->arch_info.fxsave);

    /* When we get an IRQ from the 8259PIC, we disable the IRQ, send the EOI,
     * and then able it after we're done handling the IRQ */
    if (iframe->intno >= PIC8259_IRQ0 && iframe->intno <= PIC8259_IRQ0 + 16) {
        pic8259_irq = iframe->intno - PIC8259_IRQ0;

        pic8259_disable_irq(pic8259_irq);
        pic8259_send_eoi(pic8259_irq);
    }

    struct irq_handler *hand;
    list_foreach_entry(&ident->list, hand, entry)
        (hand->callback) (iframe, hand->param);

    if (pic8259_irq >= 0)
        pic8259_enable_irq(pic8259_irq);

    if (frame_flag && t && t->sig_pending)
        signal_handle(t, iframe);

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
    if (flag_test(&t->flags, TASK_FLAG_KILLED))
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
    if (flag_test(&t->flags, TASK_FLAG_KILLED))
        sys_exit(0);

    if (cpuid_has_sse())
        i387_fxrstor(&t->arch_info.fxsave);
}

static int interrupts_seq_start(struct seq_file *seq)
{
    if (seq->iter_offset == 256)
        flag_set(&seq->flags, SEQ_FILE_DONE);

    return 0;
}

static void interrupts_seq_end(struct seq_file *seq)
{
}

static int interrupts_seq_render(struct seq_file *seq)
{
    irq_flags_t flags = irq_save();
    irq_disable();

    struct irq_handler *hand;
    list_foreach_entry(&idt_ids[seq->iter_offset].list, hand, entry)
        seq_printf(seq, "%d: %d %s\n", seq->iter_offset, atomic32_get(&idt_ids[seq->iter_offset].count), hand->id);

    irq_restore(flags);
    return 0;
}

static int interrupts_seq_next(struct seq_file *seq)
{
    seq->iter_offset++;

    if (seq->iter_offset == 256)
        flag_set(&seq->flags, SEQ_FILE_DONE);

    return 0;
}

const static struct seq_file_ops interrupts_seq_file_ops = {
    .start = interrupts_seq_start,
    .end = interrupts_seq_end,
    .render = interrupts_seq_render,
    .next = interrupts_seq_next,
};

static int interrupts_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &interrupts_seq_file_ops);
}

const struct file_ops interrupts_file_ops = {
    .open = interrupts_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
