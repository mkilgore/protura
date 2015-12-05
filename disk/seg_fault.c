

#include <protura/types.h>
#include "syscalls.h"
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

char *no_terminating_char = "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            ;


int main(int argc, char **argv)
{
    volatile char *ptr = 0;
    int ret;

    ret = open(no_terminating_char, 0, 0);

    putstr("ret=");
    putint(ret);
    putchar('\n');

    ret = read(0, NULL, 40);

    putstr("ret=");
    putint(ret);
    putchar('\n');

    ret = write(1, NULL, 410000);

    putstr("ret=");
    putint(ret);
    putchar('\n');

    *ptr = 2;
}

