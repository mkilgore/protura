/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_ERRORS_H
#define INCLUDE_PROTURA_ERRORS_H

#define ENOTDIR 1
#define ENOTSUP 2
#define EBADF   3
#define ENFILE  4
#define EMFILE  5
#define ENOENT  6
#define EINVAL  7
#define EISDIR  8
#define EFBIG   9
#define ENOEXEC 10
#define ENODEV  11

extern const char *error_strings[];

static inline const char *strerror(int err)
{
    return error_strings[-err];
}

#endif
