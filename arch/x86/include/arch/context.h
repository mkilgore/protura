#ifndef INCLUDE_ARCH_CONTEXT_H
#define INCLUDE_ARCH_CONTEXT_H

#define X86_REGS_SAVE_ON_STACK() \
    pushl %ds; \
    pushl %es; \
    pushl %fs; \
    pushl %gs; \
    pushal;

#define X86_REGS_LOAD_FROM_STACK() \
    popal; \
    popl %gs; \
    popl %fs; \
    popl %es; \
    popl %ds;


#ifndef ASM

#include <protura/types.h>
#include <protura/compiler.h>

struct x86_regs {
    uint32_t edi, esi, ebx, ebp, eip;
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
