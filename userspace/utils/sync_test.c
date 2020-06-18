
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <protura/syscall.h>

int main(int argc, char **argv)
{
    while (1)
        sync();

    return 0;
}

