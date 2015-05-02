
kernel_exit_string:
    .ascii "Kernel thread exited with: %d\n\0"

.extern kprintf


/* A separate way to start a kernel_task - This turns on interrupts before
 * continuing on to kernel_task_entry */
.globl kernel_task_entry_interruptable
kernel_task_entry_interruptable:
    sti

/* task_kernel_new already sets up the kernel stack with argv and argc already
 * pushed, along with the address of the function to call to run the kernel task.. */
.global kernel_task_entry
kernel_task_entry:

    # Push argv and argc
    pop %ebx

    # Call the kernel thread function
    call *%ebx

    # Push the exit-code from the thread
    # And display it
    pushl %eax
    pushl kernel_exit_string
    call kprintf
1: jmp 1b

.global kernel_task_entry_addr
kernel_task_entry_addr:
    .long kernel_task_entry

.globl kernel_task_entry_interruptable_addr
kernel_task_entry_interruptable_addr:
    .long kernel_task_entry_interruptable
