
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void signal_thread(void)
{
    sigset_t newset;

    sigfillset(&newset);

    sigprocmask(SIG_BLOCK, &newset, NULL);

    while (1) {
        int sig = 0, ret;
        ret = sigwait(&newset, &sig);

        printf("Sigwait: %d\n", ret);
        printf("Received signal: %d\n", sig);
    }
}

pid_t start_signal_prog(void)
{
    pid_t child;
    switch ((child = fork())) {
    case 0:
        /* child */
        signal_thread();
        exit(0);

    case -1:
        /* Err */
        return -1;

    default:
        return child;
    }
}

int main(int argc, char **argv)
{
    pid_t child;

    child = start_signal_prog();

    int i;
    for (i = 1; i <= NSIG; i++) {
        if (i != SIGKILL && i != SIGSTOP) {
            printf("Sending signal: %d\n", i);
            kill(child, i);
        }
    }

    printf("Sending SIGKILL...\n");
    /* kill(child, SIGKILL); */

    wait(NULL);

    return 0;
}

