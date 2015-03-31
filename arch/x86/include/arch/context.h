#ifndef INCLUDE_ARCH_CONTEXT_H
#define INCLUDE_ARCH_CONTEXT_H

#define X86_REGS_SAVE_ON_STACK() \
    pushl %ds; \
    pushl %es; \
    pushl %fs; \
    pushl %gs; \
    pushl;

#define X86_REGS_LOAD_FROM_STACK() \
    popal; \
    popl %gs; \
    popl %fs; \
    popl %es; \
    popl %ds;


#ifndef ASM

#include <protura/types.h>
#include <protura/compiler.h>

/* Saved registers for a context switch. */

/* A structure holding all the x86 registers 'pusha'. The order is the same as
 * a 'pusha', so the order should *not* be changed. */
struct x86_regs {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint16_t gs, _pad1, fs, _pad2, es, _pad3, ds, _pad4;
} __packed;

struct arch_context {
    /* Note: The 'save_regs' cs is going to be wrong, the actual 'cs' is down
     * below */
    struct x86_regs save_regs;
    uint16_t cs;
    uint32_t eip;
    uint32_t esp;
    uint32_t ss;
};

struct idt_frame;

void arch_context_switch(struct arch_context *old, struct arch_context *new, struct idt_frame *);

#endif

#endif
