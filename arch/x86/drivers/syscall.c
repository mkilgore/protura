
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

static void syscall_handler(struct idt_frame *frame)
{
    struct task *current = cpu_get_local()->current, *new;

    switch (frame->eax) {
    case SYSCALL_PUTCHAR:
        term_putchar(frame->ebx);
        break;
    case SYSCALL_CLOCK:
        frame->eax = timer_get_ticks();
        break;
    case SYSCALL_GETPID:
        frame->eax = current->pid;
        break;
    case SYSCALL_PUTINT:
        term_printf("%d", frame->ebx);
        break;
    case SYSCALL_PUTSTR:
        term_printf("%s", (char *)frame->ebx);
        break;
    case SYSCALL_SLEEP:
        scheduler_task_waitms(frame->ebx);
        break;
    case SYSCALL_FORK:
        new = task_fork(current);

        kprintf("New task: %d\n", new->pid);

        if (new) {
            scheduler_task_add(new);

            new->context.frame->eax = 0;
            current->context.frame->eax = new->pid;
        } else {
            current->context.frame->eax = -1;
        }

        break;
    case SYSCALL_GETPPID:
        if (current->parent)
            frame->eax = current->parent->pid;
        else
            frame->eax = -1;
        break;
    }
}

void syscall_init(void)
{
    irq_register_callback(INT_SYSCALL, syscall_handler, "syscall", IRQ_SYSCALL);
}

