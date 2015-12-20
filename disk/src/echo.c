
#include <protura/types.h>
#include <syscalls.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

int main(int argc, char **argv)
{
    write(1, begin_str, sizeof(begin_str) - 1);

    char c = '\0';
    while (c != '~') {
        read(0, &c, 1);
        write(1, &c, 1);
    }

    return 0;
}
