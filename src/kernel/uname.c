/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/snprintf.h>
#include <protura/fs/procfs.h>
#include <protura/mm/user_check.h>
#include <protura/utsname.h>

static struct utsname os_name = {
    .sysname = "Protura",
    .nodename = "",
    .release = Q(PROTURA_VERSION_FULL),
    .machine = Q(PROTURA_ARCH),
    .version = __DATE__ " " __TIME__,
};

int sys_uname(struct user_buffer utsname)
{
    return user_copy_from_kernel(utsname, os_name);
}

static int proc_version_readpage(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%s %s %s %s\n",
            os_name.sysname, os_name.release, os_name.version, os_name.machine);
    return 0;
}

struct procfs_entry_ops proc_version_ops = {
    .readpage = proc_version_readpage,
};
