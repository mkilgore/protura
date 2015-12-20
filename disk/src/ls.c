
#include <protura/types.h>
#include <protura/compiler.h>
#include <syscalls.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>
#include <protura/fs/dent.h>
#include <string.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

static inline void print_int(unsigned int val)
{
    char num_buf[15], *top = num_buf + sizeof(num_buf), *buf = top;
    const char table[] = "0123456789";

    do {
        int digit = val % 10;
        val /= 10;

        *--buf = table[digit];
    } while (val);

    write(1, buf, top - buf);
}

static inline void print_hex(unsigned int val)
{
    char num_buf[15], *top = num_buf + sizeof(num_buf), *buf = top;
    const char table[] = "0123456789ABCDEF";

    do {
        int digit = val % 16;
        val /= 16;

        *--buf = table[digit];
    } while (val);

    *--buf = 'x';
    *--buf = '0';

    write(1, buf, top - buf);
}

#define print(str) \
    write(1, (str), strlen(str))


__align(4) char dent_buffer[256];

const char *dir = "./";

int main(int argc, char **argv)
{
    int dirfd;
    int ret;

    if (argc)
        dir = argv[1];

    dirfd = open("./", O_RDONLY, 0);

    while ((ret = read_dent(dirfd, dent_buffer, sizeof(dent_buffer))) != 0) {
        struct dent *dent = (struct dent *)dent_buffer;
        print(dent->name);
        print(" ");
        print_int(dent->ino);
        print("\n");
    }

    print("ret=");
    print_int(ret);
    print("\n");

    close(dirfd);

    return 0;
}

