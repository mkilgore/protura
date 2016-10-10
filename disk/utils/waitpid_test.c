
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void child(void)
{
    printf("Child running...\n");

    while (1) {
        pause();
        printf("Continued...\n");
    }

    exit(0);
}

int main(int argc, char **argv)
{
    pid_t c;
    int wstatus;

    switch ((c = fork())) {
    case 0:
        child();
        break;

    case -1:
        perror("fork()");
        return 0;

    default:
        break;
    }

    printf("Child: %d\n", c);

    do {
        pid_t w = waitpid(c, &wstatus, WUNTRACED | WCONTINUED);
        if (w == -1) {
            perror("waitpid()");
            return 1;
        }

        if (WIFEXITED(wstatus)) {
            printf("[%d] Exit, status: %d\n", c, WEXITSTATUS(wstatus));
        } else if (WIFSIGNALED(wstatus)) {
            printf("[%d] Killed by signal: %d\n", c, WTERMSIG(wstatus));
        } else if (WIFSTOPPED(wstatus)) {
            printf("[%d] Stopped by signal: %d\n", c, WSTOPSIG(wstatus));
        } else if (WIFCONTINUED(wstatus)) {
            printf("[%d] Continued\n", c);
        }
    } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

    return 0;
}

