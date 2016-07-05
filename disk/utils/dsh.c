
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#define ARG_MAX 30

struct prog_desc {
    char *file;

    int argc;
    char **argv;

    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
};

static void start_child(const struct prog_desc *prog)
{
    int i;

    if (prog->stdin_fd != STDIN_FILENO)
        dup2(prog->stdin_fd, STDIN_FILENO);

    if (prog->stdout_fd != STDOUT_FILENO)
        dup2(prog->stdout_fd, STDOUT_FILENO);

    if (prog->stderr_fd != STDERR_FILENO)
        dup2(prog->stderr_fd, STDERR_FILENO);

    /* Close the rest of the files
     * 20 is the max file descriptor in thie case */
    for (i = 3; i < 20; i++)
        close(i);

    if (execv(prog->file, prog->argv) == -1) {
        printf("Error execing program: %s\n", prog->file);
        exit(0);
    }
}

int prog_start(const struct prog_desc *prog, pid_t *child_pid)
{
    pid_t pid = fork();

    if (pid == -1)
        return 1; /* fork() returned an error - abort */

    if (pid == 0) /* 0 is returned to the child process */
        start_child(prog);

    *child_pid = pid;
    return 0;
}


int main(int argc, char **argv)
{
    struct prog_desc ls, head;
    pid_t ls_pid, head_pid;
    int fds[2];

    ls.file = "/bin/ls";
    ls.argc = 1;
    ls.argv = (char *[]) { "/bin/ls", NULL };
    ls.stdin_fd = STDIN_FILENO;
    ls.stderr_fd = STDERR_FILENO;

    head.file = "/bin/head";
    head.argc = 1;
    head.argv = (char *[]) { "/bin/head", NULL };
    head.stdout_fd= STDOUT_FILENO;
    head.stderr_fd = STDERR_FILENO;

    pipe(fds);

    ls.stdout_fd = fds[1];
    head.stdin_fd = fds[0];

    prog_start(&ls, &ls_pid);
    prog_start(&head, &head_pid);

    close(fds[1]);
    close(fds[0]);

    wait(NULL);
    wait(NULL);

    return 0;
}

