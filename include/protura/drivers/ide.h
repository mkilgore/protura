/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_IDE_H
#define INCLUDE_DRIVERS_IDE_H

#include <protura/drivers/pci.h>
#include <protura/fs/block.h>

void ide_init(void);

/* Block should be locked before syncing */
void ide_sync_block_master(struct block_device *, struct block *);
void ide_sync_block_slave(struct block_device *, struct block *);

extern struct block_device_ops ide_master_block_device_ops;
extern struct block_device_ops ide_slave_block_device_ops;

#endif
