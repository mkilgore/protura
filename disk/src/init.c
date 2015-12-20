
#include <protura/types.h>
#include <syscalls.h>
#include <string.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>

#define hello "Hello from Init!!!\n"

#define prompt "echo=e seg-fault-test=s brk-test=b ls=l a=arg_test\n"

static pid_t start_prog(const char *prog, const char *const argv[])
{
    pid_t child_pid;

    switch ((child_pid = fork())) {
    case -1:
        /* Fork error */
        return -1;

    case 0:
        /* In child */
        execv(prog, argv);
        _exit(0);

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

    link("/test_file", "/test_file_linked1");
    link("/test_file", "/test_file_linked2");
    link("/test_file", "/test_file_linked3");
    link("/test_file", "/test_file_linked4");
    link("/test_file", "/test_file_linked5");
    link("/test_file", "/test_file_linked6");
    link("/test_file", "/test_file_linked7");
    link("/test_file", "/test_file_linked8");
    link("/test_file", "/test_file_linked9");

    write(consolefd, hello, sizeof(hello) - 1);

    write(consolefd, prompt, sizeof(prompt) - 1);

    while (1) {
        char c;
        read(keyboardfd, &c, 1);

        if (c == 'e') {
            close(consolefd);
            consolefd = open("/test_file", O_WRONLY | O_APPEND, 0);

            start_prog("/bin/echo", NULL);
            wait(NULL);

            close(consolefd);
            consolefd = open("/dev/com2", O_WRONLY, 0);
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
            start_prog("/bin/arg_test", (const char *const []) { "Argument 1", "Argument 2", "Argument 3", "-c", "-w2", "--make", NULL } );
            wait(NULL);
        }

        sync();

        write(consolefd, prompt, sizeof(prompt) - 1);
    }

    return 0;
}

