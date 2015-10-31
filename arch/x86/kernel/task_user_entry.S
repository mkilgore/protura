
.extern sys_exec

.globl arch_task_user_entry
arch_task_user_entry:

    call sys_exec

    addl $12, %esp

    ret

.global arch_task_user_entry_addr
arch_task_user_entry_addr:
    .long arch_task_user_entry

