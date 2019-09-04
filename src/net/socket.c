/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/socket.h>
#include <protura/net.h>

atomic_t open_sockets = ATOMIC_INIT(0);

struct socket *socket_alloc(void)
{
    struct socket *socket;

    socket = kzalloc(sizeof(*socket), PAL_KERNEL);
    socket_init(socket);
    atomic_inc(&open_sockets);

    return socket_dup(socket);
}

void socket_free(struct socket *socket)
{
    atomic_dec(&open_sockets);
    kfree(socket);
}

static int socket_proc_read(void *page, size_t page_size, size_t *len)
{
    int sockets = atomic_get(&open_sockets);

    *len = snprintf(page, page_size, "Sockets: %d\n", sockets);

    return 0;
}

struct procfs_entry_ops socket_procfs = {
    .readpage = socket_proc_read,
};

