
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

sig_atomic_t volatile quit = 0;

void handle_sig(int sig)
{
    printf("Signal: %d\n", sig);
    quit = 1;
}

pid_t start_signal_prog(void)
{
    pid_t child, parent;
    switch ((child = fork())) {
    case 0:
        /* child */
        parent = getppid();
        kill(parent, SIGINT);
        kill(parent, SIGINT);
        kill(parent, SIGINT);
        kill(parent, SIGINT);
        kill(parent, SIGINT);
        kill(parent, SIGINT);
        kill(parent, SIGINT);

        printf("Sleeping...\n");
        sleep(10);

        kill(parent, SIGUSR1);
        exit(0);

    case -1:
        /* Err */
        return -1;

    default:
        break;
    }

    return child;
}

int main(int argc, char **argv)
{
    pid_t child;
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sig;

    sigaction(SIGUSR1, &action, NULL);

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;

    sigaction(SIGUSR2, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    child = start_signal_prog();

    printf("Pausing...\n");
    pause();

    printf("Pause exited!!!\n");
    printf("Quit=%d\n", quit);

    waitpid(child, NULL, 0);

    return 0;
}

