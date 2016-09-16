/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_FS_FDSET_H
#define INCLUDE_PROTURA_FS_FDSET_H

#if __KERNEL__
# include <protura/types.h>
# include <arch/bitops.h>
#endif

#define NOFILE 32
#define FD_SETSIZE 32

typedef unsigned int fd_set;
typedef unsigned int fd_mask;

#define NBBY 8 /* Number of bits in a byte */

#define NFDBITS (sizeof(fd_mask) * NBBY)

static inline void FD_CLR(int fd, fd_set *set)
{
    *set &= ~(1 << fd);
}

static inline void FD_SET(int fd, fd_set *set)
{
    *set |= (1 << fd);
}

static inline int FD_ISSET(int fd, fd_set *set)
{
    return !!(*set & (1 << fd));
}

static inline void FD_ZERO(fd_set *set)
{
    *set = 0;
}

#endif
