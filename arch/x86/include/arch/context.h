#ifndef INCLUDE_ARCH_CONTEXT_H
#define INCLUDE_ARCH_CONTEXT_H

#ifndef ASM

#include <protura/types.h>
#include <protura/compiler.h>
#include <protura/irq.h>

/* Note: Dont' change this unless you update context.S as well */
struct x86_regs {
    uint32_t edi, esi, ebx, ebp, eip;
};

/* Note: 'arch_context_switch' assembly depends on 'x86_regs' being the first
 * entry, and 'eflags' being the second entry.
 *
 * eflags is separate because the spinlock that locks the ktasks list has to
 * mess with the eflags to ensure they are disabled when the lock is taken.
 */
struct arch_context {
    struct x86_regs *esp;
    uint32_t eflags;
    struct irq_frame *frame;
};

void arch_context_switch(struct arch_context *new, struct arch_context *old);

typedef struct arch_context context_t;

#endif

#endif
