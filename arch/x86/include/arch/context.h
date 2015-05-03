#ifndef INCLUDE_ARCH_CONTEXT_H
#define INCLUDE_ARCH_CONTEXT_H

#ifndef ASM

#include <protura/types.h>
#include <protura/compiler.h>

/* Note: Dont' change this unless you update context.S as well */
struct x86_regs {
    uint32_t edi, esi, ebx, ebp, eflags, eip;
};

/* Note: 'arch_context_switch' assembly depends on 'x86_regs' being the first
 * entry */
struct arch_context {
    struct x86_regs *esp;
    struct idt_frame *frame;
};

void arch_context_switch(struct arch_context *new, struct arch_context *old);

#endif

#endif
