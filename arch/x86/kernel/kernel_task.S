
.extern sys_exit

/* task_kernel_new already sets up the kernel stack with the params already
 * pushed, along with the address of the function to call to run the kernel
 * task. We simply call that function, and then handle clean-up when the thread
 * exits via returning. */
.globl kernel_task_entry
.type kernel_task_entry, @function
kernel_task_entry:
    sti

    pop %ebx

    /* Call the kernel thread function */
    call *%ebx

    /* Call sys_exit() to close our Kernel thread. %eax is the return code
     * from the thread, passed as an argument to sys_exit(). */
    push %eax
    call sys_exit
    /* Loop just in case there is a problem -- In working conditions sys_exit
     * should never return */
1: jmp 1b
.size kernel_task_entry, .-kernel_task_entry

.global kernel_task_entry_addr
kernel_task_entry_addr:
    .long kernel_task_entry
