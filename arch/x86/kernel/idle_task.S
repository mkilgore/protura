
.global kernel_arch_idle_task_entry
kernel_arch_idle_task_entry:
1:  hlt
    jmp 1b

kernel_arch_idle_task_entry_end:

.globl kernel_arch_idle_task_size
kernel_arch_idle_task_size:
    .long kernel_arch_idle_task_entry_end - kernel_arch_idle_task_entry

