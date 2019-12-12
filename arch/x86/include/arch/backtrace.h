#ifndef INCLUDE_ARCH_BACKTRACE_H
#define INCLUDE_ARCH_BACKTRACE_H

#include <protura/types.h>
#include <protura/compiler.h>

void dump_stack(int log_level);
void dump_stack_ptr(void *start, int log_level);

struct stackframe {
    struct stackframe *caller_stackframe;
    uintptr_t return_addr;
} __packed;

/* The make-up of an x86 stackframe.
 * Note: 'caller_stackframe' is the 'ebp' of the caller. */
#define read_ebp() ({ void *__ebp; \
                      asm volatile("movl %%ebp, %0": "=r" (__ebp)::); \
                      __ebp; })

#define read_return_addr() \
    ({ \
        struct stackframe *__stackframe = read_ebp(); \
        __stackframe->return_addr; \
    })

#endif
