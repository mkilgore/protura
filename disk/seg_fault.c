

#include <protura/types.h>
#include "syscalls.h"
#include <fs/stat.h>
#include <fs/fcntl.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

int main(int argc, char **argv)
{
    volatile char *ptr = 0;
    *ptr = 2;
}

