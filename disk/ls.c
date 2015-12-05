
#include <protura/types.h>
#include <protura/compiler.h>
#include "syscalls.h"
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>
#include <protura/fs/dent.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

__align(4) char dent_buffer[256];

int main(int argc, char **argv)
{
    int dirfd = open("/", O_RDONLY, 0);

    while ((read_dent(dirfd, dent_buffer, sizeof(dent_buffer))) != 0) {
        struct dent *dent = (struct dent *)dent_buffer;
        putstr(dent->name);
        putstr(" ");
        putint(dent->ino);
        putchar('\n');
    }

    exit(0);
}

