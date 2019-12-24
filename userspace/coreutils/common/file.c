
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

int open_with_dash(const char *path, int flags, ...)
{
    if (strcmp(path, "-") == 0)
        return STDIN_FILENO;

    if (flags & O_CREAT) {
        va_list list;
        int mode;
        va_start(list, flags);
        mode = va_arg(list, int);
        va_end(list);
        return open(path, flags, mode);
    }

    return open(path, flags);
}

void close_with_dash(int fd)
{
    if (fd != STDIN_FILENO)
        close(fd);
}

FILE *fopen_with_dash(const char *path, const char *mode)
{
    if (strcmp(path, "-") == 0)
        return stdin;

    return fopen(path, mode);
}

void fclose_with_dash(FILE *file)
{
    if (file != stdin)
        fclose(file);
}

