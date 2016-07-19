
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

int main()
{
    int i = 0;
    for (i = 0; i < 10; i++) {
        pid_t child;

        if ((child = fork()) == 0)
            exit(0);

        printf("Child: %d\n", child);
    }

    /* We exit *without* waiting on our children */
    return 0;
}

