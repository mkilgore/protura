/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_FS_FCNTL_H__
#define __INCLUDE_UAPI_PROTURA_FS_FCNTL_H__

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR   0x03
#define O_APPEND 0x08
#define O_CREAT  0x0200
#define O_TRUNC  0x0400
#define O_EXCL   0x0800
#define O_NOCTTY 0x1000
#define O_NONBLOCK 0x4000
#define O_NDELAY   O_NONBLOCK
#define O_CLOEXEC 0x40000

#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

#define FD_CLOEXEC 1

#endif
