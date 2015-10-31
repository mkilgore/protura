
int test[50];

int test2 = 4;

#define SYSCALL_PUTCHAR 0x01
#define SYSCALL_SLEEP 0x06

static void syscall(int sys, int arg1, int arg2)
{
    asm volatile("movl %0, %%eax\n"
                 "movl %1, %%ebx\n"
                 "movl %2, %%ecx\n"
                 "int $0x81\n"
                 :
                 : "r" (sys), "r" (arg1), "r" (arg2)
                 : "memory", "%eax", "%ebx", "%ecx");
}

int main(int argc, char **argv)
{
    while (1) {
        syscall(SYSCALL_PUTCHAR, 'a', 0);
        syscall(SYSCALL_SLEEP, 1000, 0);
    }

    test[2] = 20;

    return 0;
}

