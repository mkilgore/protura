
//#include <syscalls.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <protura/syscall.h>

#define hello "Hello from Init!!!\n"

#define prompt "echo=e seg-fault-test=s brk-test=b ls=l a=arg_test p=pipe_test\n"

static pid_t start_prog(const char *prog, char *const argv[], char *const envp[])
{
    pid_t child_pid;

    switch ((child_pid = fork())) {
    case -1:
        /* Fork error */
        return -1;

    case 0:
        /* In child */
        execve(prog, argv, envp);
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

    start_prog("/bin/echo", NULL, NULL);

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
            start_prog("/bin/echo", NULL, NULL);
            wait(NULL);
        }

        if (c == 's') {
            start_prog("/bin/seg_fault", NULL, NULL);
            wait(NULL);
        }

        if (c == 'b') {
            start_prog("/bin/brk_test", NULL, NULL);
            wait(NULL);
        }

        if (c == 'l') {
            start_prog("/bin/ls", NULL, NULL);
            wait(NULL);
        }

        if (c == 'a') {
            start_prog("/bin/arg_test", (char *const []) { "Argument 1", "Argument 2", "Argument 3", "-c", "-w2", "--make", NULL }, (char *const[]) { "PATH=/usr/bin", "test=200", NULL } );
            wait(NULL);
        }

        if (c == 'p') {
            start_prog("/bin/pipe_test", NULL, NULL);
            wait(NULL);
        }

        sync();

        write(consolefd, prompt, sizeof(prompt) - 1);
    }

    return 0;
}

