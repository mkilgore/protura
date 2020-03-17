/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_FS_FDSET_H__
#define __INCLUDE_UAPI_PROTURA_FS_FDSET_H__

#include <protura/types.h>

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

#endif
