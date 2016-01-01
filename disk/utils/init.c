
//#include <syscalls.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <protura/syscall.h>

#define hello "Hello from Init!!!\n"

#define prompt "echo=e seg-fault-test=s brk-test=b ls=l a=arg_test\n"

static inline int syscall0(int sys)
{
    int out;

    asm volatile("int $0x81"
                 : "=a" (out)
                 : "0" (sys)
                 : "memory");

    return out;
}

/*pid_t fork(void)
{
    return syscall0(SYSCALL_FORK);
} */

static pid_t start_prog(const char *prog, char *const argv[])
{
    pid_t child_pid;

    switch ((child_pid = fork())) {
    case -1:
        /* Fork error */
        return -1;

    case 0:
        /* In child */
        execve(prog, argv, NULL);
        exit(0);

    default:
        /* In parent */
        return child_pid;
    }
}

int main(int argc, char **argv)
{
    int consolefd, keyboardfd;

    keyboardfd = open("/dev/keyboard", O_RDONLY, 0);
    consolefd = open("/dev/console", O_WRONLY, 0);

    start_prog("/bin/echo", NULL);

    close(keyboardfd);
    close(consolefd);

    keyboardfd = open("/dev/com2", O_RDONLY, 0);
    consolefd = open("/dev/com2", O_WRONLY, 0);

    write(consolefd, hello, sizeof(hello) - 1);

    write(consolefd, prompt, sizeof(prompt) - 1);

    printf("keyboardfd: %d\n", keyboardfd);
    printf("consolefd: %d\n", consolefd);

    fwrite("Test\n", 5, 1, stdout);
    fflush(stdout);

    sync();

    while (1) {
        char c;
        read(keyboardfd, &c, 1);

        if (c == 'e') {
            start_prog("/bin/echo", NULL);
            wait(NULL);
        }

        if (c == 's') {
            start_prog("/bin/seg_fault", NULL);
            wait(NULL);
        }

        if (c == 'b') {
            start_prog("/bin/brk_test", NULL);
            wait(NULL);
        }

        if (c == 'l') {
            start_prog("/bin/ls", NULL);
            wait(NULL);
        }

        if (c == 'a') {
            start_prog("/bin/arg_test", (char *const [20]) { "Argument 1", "Argument 2", "Argument 3", "-c", "-w2", "--make", NULL } );
            wait(NULL);
        }

        sync();

        write(consolefd, prompt, sizeof(prompt) - 1);
    }

    return 0;
}

