
#include <protura/types.h>
#include <syscalls.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>
#include <string.h>

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
    write(1, (str), sizeof(str) - 1)

int main(int argc, char **argv)
{
    int i;

    print("argc: ");
    print_int(argc);
    print("\n");

    for (i = 0; i < argc; i++) {
        print("str: ");
        write(1, argv[i], strlen(argv[i]));
        print("\n");
    }

    argv[0][1] = 'k';

    print("Modified arg: ");
    write(1, argv[0], strlen(argv[0]));
    print("\n");

    return 0;
}

