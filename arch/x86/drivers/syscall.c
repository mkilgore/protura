
#include <protura/types.h>
#include <arch/idt.h>
#include <drivers/term.h>
#include <arch/syscall.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <arch/drivers/pic8259_timer.h>

static void syscall_handler(struct idt_frame *frame)
{
    switch (frame->regs.eax) {
    case SYSCALL_PUTCHAR:
        term_putchar(frame->regs.ebx);
        break;
    case SYSCALL_CLOCK:
        frame->regs.eax = timer_get_ticks();
        break;
    case SYSCALL_GETPID:
        frame->regs.eax = cpu_get_local()->current->pid;
        break;
    case SYSCALL_PUTINT:
        term_printf("%d", frame->regs.ebx);
        break;
    case SYSCALL_PUTSTR:
        term_printf("%s", (char *)frame->regs.ebx);
        break;
    }
}

void syscall_init(void)
{
    irq_register_callback(INT_SYSCALL, syscall_handler, "syscall");
}

