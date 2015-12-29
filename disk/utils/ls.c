/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

//#include <syscalls.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

struct dent {
    uint32_t ino;
    uint32_t dent_len;
    uint32_t name_len;
    char name[];
};

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


__attribute__((align(4))) char dent_buffer[256];

const char *dir = "./";

int main(int argc, char **argv)
{
    int dirfd;
    int ret;

    if (argc)
        dir = argv[1];

    dirfd = open("./", O_RDONLY, 0);

    while ((ret = syscall3(SYSCALL_READ_DENT, dirfd, dent_buffer, sizeof(dent_buffer))) != 0) {
        struct dent *dent = (struct dent *)dent_buffer;
        printf("%s %d\n", dent->name, dent->ino);
    }

    printf("ret = %d\n", ret);

    close(dirfd);

    return 0;
}

