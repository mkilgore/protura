
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>

jmp_buf jmp;

void sigint_hand(int signum)
{
    sigset_t set;
    printf("Signal: %d\n", signum);

    sigemptyset(&set);
    sigaddset(&set, signum);
    sigprocmask(SIG_UNBLOCK, &set, 0);

    longjmp(jmp, 1);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigint_hand);

    if (setjmp(jmp)) {
        printf("Jumped back to main!");
    } else {
        printf("Waiting for signal...\n");
        while (1)
            pause();
    }

    return 0;
}

