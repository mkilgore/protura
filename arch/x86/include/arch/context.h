#ifndef INCLUDE_ARCH_CONTEXT_H
#define INCLUDE_ARCH_CONTEXT_H

#include <protura/types.h>
#include <protura/compiler.h>

/* Saved registers for a context switch.
 *
 * Saving %eax, %ecx, %edx is unnessisary because the caller saves them.
 *
 * When context-switching, the context itself is saved to the task it
 * describes, so the address of the ocntext is the address of that stack and
 * that context's %esp. */
struct arch_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
} __packed;

void arch_context_switch(struct arch_context **old, struct arch_context *new);

#endif
