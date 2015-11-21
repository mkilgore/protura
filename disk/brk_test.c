
#include <protura/types.h>
#include "syscalls.h"
#include <fs/stat.h>
#include <fs/fcntl.h>

int data1 = 1;

int main(int argc, char **argv)
{
    int i;
    int *ints;

    putint(data1);
    putchar('\n');

    sbrk(4096);

    ints = sbrk(20 * sizeof(int));

    for (i = 0; i < 20; i++) {
        ints[i] = i + 50;
        putint(ints[i]);
        putchar('\n');
    }

    exit(0);
}
