
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define INT_COUNT 256

int main(int argc, char **argv)
{
    int i;
    int *ints;

    int fd = open("/dev/ttyS1", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    while (1) {
        write(STDOUT_FILENO, "Starting...\n", 12);
        sleep(2);
        write(STDOUT_FILENO, "Calling sbrk...\n", 16);
        ints = malloc(INT_COUNT * sizeof(int));

        printf("New malloc ptr: %p\n", ints);

        printf("Sleeping for 10 seconds after allocation...\n");
        sleep(10);
        printf("Faulting page in...\n");

        for (i = 0; i < INT_COUNT; i++) {
            printf("Prev value: %d, ", ints[i]);
            ints[i] = i;
            printf("new %d\n", ints[i]);
        }
        printf("Writing done...\n");
        printf("Argv 0: %s\n", argv[0]);
    }

    return 0;
}
