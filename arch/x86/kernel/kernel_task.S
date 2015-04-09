
kernel_exit_string:
    .ascii "Kernel thread exited with: %d\n\0"

.extern kprintf

.global kernel_task_entry
kernel_task_entry:

    # Push argv and argc
    pushl %eax
    pushl %ebx

    # Call the kernel thread function
    call *%ecx

    # Push the exit-code from the thread
    # And display it
    pushl %eax
    pushl kernel_exit_string
    call kprintf
1: jmp 1b

.global kernel_task_entry_addr
kernel_task_entry_addr:
    .long kernel_task_entry

