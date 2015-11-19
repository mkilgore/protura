
kernel_exit_string:
    .ascii "Kernel thread exited with: %d\n\0"

.extern kprintf
.extern scheduler_task_dead


/* A separate way to start a kernel_task - This turns on interrupts before
 * continuing on to kernel_task_entry */
.globl kernel_task_entry_interruptable
kernel_task_entry_interruptable:
    sti

/* task_kernel_new already sets up the kernel stack with argv and argc already
 * pushed, along with the address of the function to call to run the kernel task.. */
.global kernel_task_entry
kernel_task_entry:

    pop %ebx

    /* Call the kernel thread function */
    call *%ebx

    /* Call 'scheduler_task_dead()' Directly */
    call scheduler_task_dead
    /* Loop just in case there is a problem -- In working conditions dead should never return */
1: jmp 1b

.global kernel_task_entry_addr
kernel_task_entry_addr:
    .long kernel_task_entry

.globl kernel_task_entry_interruptable_addr
kernel_task_entry_interruptable_addr:
    .long kernel_task_entry_interruptable
