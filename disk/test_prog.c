
#include <protura/types.h>
#include "syscalls.h"
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>

int test[50];

char test_char;

#define console_str "Writing to /dev/console!!!!!!\n"

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
    int procs = 50, i;

    keyboardfd = open("/dev/keyboard", O_RDONLY, 0);
    consolefd = open("/dev/console", O_WRONLY, 0);

    write(consolefd, console_str, sizeof(console_str) - 1);

    exec("/echo");

    int ret = 0;
    while (1) {
        pid_t c_pid, wait_pid;

        for (i = 0; i < procs; i++)
            c_pid = start_prog("/exec_prog");

        for (i = 0; i < procs; i++) {
            wait_pid = wait(NULL);
            putstr("Done waiting for: ");
            putint(wait_pid);
            putchar('\n');
        }
        ret++;
        /*
        c_pid = start_prog("/exec_prog");

        wait_pid = wait(&ret);
        putstr("PID: ");
        putint(wait_pid);
        putchar('\n'); */

    }

    return 0;
}

