
#include <stdio.h>
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

    printf("argc: %d\n", argc);

    for (i = 0; i < argc; i++) {
        printf("str: %s\n", argv[i]);
    }

    argv[0][1] = 'k';

    printf("Modified arg: %s\n", argv[0]);

    return 0;
}

