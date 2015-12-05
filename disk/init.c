
#include <protura/types.h>
#include "syscalls.h"
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>

#define hello "Hello from Init!!!\n"

#define prompt "echo=e seg-fault-test=s brk-test=b ls=l\n"

static pid_t start_prog(const char *prog)
{
    pid_t child_pid;

    switch ((child_pid = fork())) {
    case -1:
        /* Fork error */
        break;

    case 0:
        /* In child */
        exec(prog);
        break;

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

    start_prog("/echo");

    close(keyboardfd);
    close(consolefd);

    keyboardfd = open("/dev/com2", O_RDONLY, 0);
    consolefd = open("/dev/com2", O_WRONLY, 0);

    write(consolefd, hello, sizeof(hello) - 1);

    write(consolefd, prompt, sizeof(prompt) - 1);
    while (1) {
        char c;
        read(keyboardfd, &c, 1);

        if (c == 'e') {
            start_prog("/echo");
            wait(NULL);
        }

        if (c == 's') {
            start_prog("/seg_fault");
            wait(NULL);
        }

        if (c == 'b') {
            start_prog("/brk_test");
            wait(NULL);
        }

        if (c == 'l') {
            start_prog("/ls");
            wait(NULL);
        }

        write(consolefd, prompt, sizeof(prompt) - 1);
    }
}

