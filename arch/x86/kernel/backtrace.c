
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/compiler.h>

#include <arch/backtrace.h>

/* The make-up of an x86 stackframe.
 * Note: 'caller_stackframe' is the 'ebp' of the caller. */
struct stackframe {
    struct stackframe *caller_stackframe;
    uintptr_t return_addr;
} __packed;

#define read_ebp() ({ void *__ebp; \
                      asm volatile("movl %%ebp, %0": "=r" (__ebp)::); \
                      __ebp; })

void dump_stack_ptr(void *start)
{
    struct stackframe *stack = start;
    int frame = 0;

    for (; stack != 0; stack = stack->caller_stackframe) {
        frame++;
        kprintf("  [%d][0x%x]\n", frame, stack->return_addr);
    }
}

void dump_stack(void)
{
    dump_stack_ptr(read_ebp());
}

