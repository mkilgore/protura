
.extern sys_execve
.extern __panic

user_init_panic:
.asciz "[PANIC]: User init failed to exec: %d\n"

.globl arch_task_user_entry
arch_task_user_entry:

    call sys_execve
    cmpl $0, %eax
    jnz bad_user_exec

    /* Clean-up the stack from the sys_execve call - Pop-off the arguments */
    addl $16, %esp

    ret

bad_user_exec:
    push %eax
    push user_init_panic
    call __panic

.global arch_task_user_entry_addr
arch_task_user_entry_addr:
    .long arch_task_user_entry

