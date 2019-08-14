
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define INT_COUNT 128

int main(int argc, char **argv)
{
    int i;
    int *ints;

    sleep(10);
    printf("Calling sbrk...\n");
    ints = sbrk(INT_COUNT * sizeof(int));

    printf("Sleeping for 20 seconds after allocation...\n");
    sleep(20);

    for (i = 0; i < INT_COUNT; i++) {
        ints[i] = i;
        printf("%d\n", ints[i]);
    }

    return 0;
}
