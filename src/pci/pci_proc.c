/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/snprintf.h>

#include <protura/fs/procfs.h>
#include <protura/fs/seq_file.h>
#include <protura/drivers/pci.h>

#include "internal.h"

static int proc_seq_start(struct seq_file *seq)
{
    return seq_list_start(seq, &pci_dev_list);
}

static int proc_seq_render(struct seq_file *seq)
{
    struct pci_dev_entry *entry = seq_list_get_entry(seq, struct pci_dev_entry, pci_dev_node);
    const char *cla = NULL, *sub = NULL;

    pci_get_class_name(entry->class, entry->subclass, &cla , &sub);

    if (sub)
        return seq_printf(seq, "%02d:%02d.%d: 0x%04x:0x%04x: %s, %s\n", entry->id.bus, entry->id.slot, entry->id.func, entry->vendor, entry->device, cla, sub);
    else
        return seq_printf(seq, "%02d:%02d.%d: 0x%04x:0x%04x: %s\n", entry->id.bus, entry->id.slot, entry->id.func, entry->vendor, entry->device, cla);
}

static int proc_seq_next(struct seq_file *seq)
{
    return seq_list_next(seq, &pci_dev_list);
}

const static struct seq_file_ops pci_seq_file_ops = {
    .start = proc_seq_start,
    .render = proc_seq_render,
    .next = proc_seq_next,
};

static int pci_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &pci_seq_file_ops);
}

const struct file_ops pci_file_ops = {
    .open = pci_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
