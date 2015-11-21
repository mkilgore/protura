
#include <protura/types.h>
#include "syscalls.h"
#include <fs/stat.h>
#include <fs/fcntl.h>

#define hello "Hello from Init!!!\n"

#define prompt "echo=e seg-fault-test=s brk-test=b\n"

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

    write(consolefd, hello, sizeof(hello) - 1);

    write(consolefd, prompt, sizeof(prompt) - 1);
    while (1) {
        char c;
        read(keyboardfd, &c, 1);

        if (c == 'e') {
            start_prog("/echo");
            wait(NULL);
            write(consolefd, prompt, sizeof(prompt) - 1);
        }

        if (c == 's') {
            start_prog("/seg_fault");
            wait(NULL);
            write(consolefd, prompt, sizeof(prompt) - 1);
        }

        if (c == 'b') {
            start_prog("/brk_test");
            wait(NULL);
            write(consolefd, prompt, sizeof(prompt) - 1);
        }
    }
}

