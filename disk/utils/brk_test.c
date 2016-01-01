
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <protura/syscall.h>

#define INT_COUNT 128

int main(int argc, char **argv)
{
    int i;
    int *ints;

    ints = sbrk(INT_COUNT * sizeof(int));

    for (i = 0; i < INT_COUNT; i++) {
        ints[i] = i;
        printf("%d\n", ints[i]);
    }

    return 0;
}
