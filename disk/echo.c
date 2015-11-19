
#include <protura/types.h>
#include "syscalls.h"
#include <fs/stat.h>
#include <fs/fcntl.h>

int main(int argc, char **argv)
{
    char buf[50], *b;
    putstr("Echo!\n");

    char c;
    while (1) {
        read(0, &c, 1);
        write(1, &c, 1);
        /*
        b = buf;
        while (read(0, &c, 1) == 1) {
            write(1, &c, 1);
            *b++ = c;
            if (c == '\n')
                break;
        }

        write(1, buf, b - buf); */
    }

    return 0;
}
