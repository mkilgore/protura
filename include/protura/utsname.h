/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_UTSNAME_H
#define INCLUDE_PROTURA_UTSNAME_H

#include <protura/types.h>

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

#ifdef __KERNEL__

int sys_uname(struct user_buffer utsname);
extern struct procfs_entry_ops proc_version_ops;

#endif

#endif
