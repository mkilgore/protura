
# arch_context_switch(struct arch_context **old, struct arch_context *new)

# Note: The pushes are in the same order as struct arch_context
# Note: %eip is already pushed on the stack when we're called
.globl arch_context_switch
arch_context_switch:
    movl 4(%esp), %eax
    movl 8(%esp), %edx

    # Save registers, same order as struct arch_context
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    # Save the location of the new struct arch_context in *old
    movl %esp, (%eax)

    # Move the 'new' context into the stack, %esp. Pop the registers saved on
    # that stack
    movl %edx, %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    # Return, but return to where the new stack was called from.
    ret

