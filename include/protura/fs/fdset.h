/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_FS_FDSET_H
#define INCLUDE_PROTURA_FS_FDSET_H

#ifdef __KERNEL__
# include <protura/types.h>
# include <arch/bitops.h>
#endif

#define __kNOFILE 32
#define __kFD_SETSIZE 32

typedef unsigned int __kfd_set;
typedef unsigned int __kfd_mask;

#define __kNBBY 8 /* Number of bits in a byte */

#define __kNFDBITS (sizeof(fd_mask) * NBBY)

static inline void __kFD_CLR(int fd, __kfd_set *set)
{
    *set &= ~(1 << fd);
}

static inline void __kFD_SET(int fd, __kfd_set *set)
{
    *set |= (1 << fd);
}

static inline int __kFD_ISSET(int fd, __kfd_set *set)
{
    return !!(*set & (1 << fd));
}

static inline void __kFD_ZERO(__kfd_set *set)
{
    *set = 0;
}

#ifdef __KERNEL__

#define NOFILE __kNOFILE
#define FD_SETSIZE __kFD_SETSIZE

typedef __kfd_set fd_set;
typedef __kfd_mask fd_mask;

#define NBBY __kNBBY
#define FDBITS __kFDBITS

#define FD_CLR(fd, set) __kFD_CLR(fd, set)
#define FD_SET(fd, set) __kFD_SET(fd, set)
#define FD_ISSET(fd, set) __kFD_ISSET(fd, set)
#define FD_ZERO(set) __kFD_ZERO(set)

#endif

#endif
