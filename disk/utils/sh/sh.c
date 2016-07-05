
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "input.h"

void handle_child(int sig)
{
    pid_t pid = 1;
    do {
        /* pid = waitpid(-1, NULL, WNOHANG); */
    } while (pid == 0);
}

void ignore_sig(int sig);

void handle_sigint(int sig)
{
    pid_t pid = getpid();
    /* "disable" SIGINT on the main thread so we don't kill the shell when we
     * kill our children */
    signal(SIGINT, ignore_sig);
    kill(-pid, SIGINT);
}

void ignore_sig(int sig)
{
    /* Restore SIGINT handler - We "disable" it by setting it to calling this
     * function when we use SIGINT to kill off all of our children */
    signal(SIGINT, handle_sigint);
}

int main(int argc, char **argv)
{
    setvbuf(stdin, NULL, _IONBF, 0);

    signal(SIGCHLD, handle_child);
    signal(SIGINT, handle_sigint);
    input_loop();
    return 0;
}

