

.globl arch_context_switch
arch_context_switch:
    movl 4(%esp), %edx /* New context */
    movl 8(%esp), %eax /* Old context */

    /* Save registers that aren't aready saved via the call in here */
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    /* Save and switch stacks - This acesses the 'esp' field of arch_context. */
    movl %esp, (%eax)
    movl (%edx), %esp

    /* Pop values for the new context off of the stack */
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

