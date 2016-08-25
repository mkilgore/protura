/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_FS_FDSET_H
#define INCLUDE_PROTURA_FS_FDSET_H

#include <protura/types.h>
#include <arch/bitops.h>

#define NOFILE 32

typedef uint32_t fd_set;

static inline void FD_CLR(int fd, fd_set *set)
{
    bit_clear(set, fd);
}

static inline void FD_SET(int fd, fd_set *set)
{
    bit_set(set, fd);
}

static inline int FD_ISSET(int fd, fd_set *set)
{
    return bit_test(set, fd);
}

static inline void FD_ZERO(fd_set *set)
{
    *set = 0;
}

#endif
