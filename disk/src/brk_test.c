
#include <protura/types.h>
#include <syscalls.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>
#include <stdlib.h>
#include <string.h>

#define INT_COUNT 128

int main(int argc, char **argv)
{
    int i;
    int *ints;

    ints = sbrk(INT_COUNT * sizeof(int));

    for (i = 0; i < INT_COUNT; i++) {
        ints[i] = i;
        putint(ints[i]);
        putchar('\n');
    }

    return 0;
}
