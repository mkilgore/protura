#ifndef COMMON_FILE_H
#define COMMON_FILE_H

#include <stdio.h>

int open_with_dash(const char *path, int flags, ...);
void close_with_dash(int fd);
FILE *fopen_with_dash(const char *path, const char *mode);
void fclose_with_dash(FILE *file);

#endif
