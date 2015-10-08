
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/compiler.h>

#include <arch/backtrace.h>

void dump_stack_ptr(void *start)
{
    struct stackframe *stack = start;
    int frame = 0;

    kprintf("  Stack: %p\n", start);

    for (; stack != 0; stack = stack->caller_stackframe) {
        frame++;
        kprintf("  [%d][0x%08x]\n", frame, stack->return_addr);
    }
}

void dump_stack(void)
{
    dump_stack_ptr(read_ebp());
}

