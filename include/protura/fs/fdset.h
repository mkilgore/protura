/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_FS_FDSET_H
#define INCLUDE_PROTURA_FS_FDSET_H

#include <uapi/protura/fs/fdset.h>

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
