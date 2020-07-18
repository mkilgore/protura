/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_ATA_H
#define INCLUDE_DRIVERS_ATA_H

#include <protura/drivers/pci.h>
#include <protura/fs/block.h>

void ata_init(void);

/* Block should be locked before syncing */
void ata_sync_block_master(struct block_device *, struct block *);
void ata_sync_block_slave(struct block_device *, struct block *);

extern struct block_device_ops ata_master_block_device_ops;
extern struct block_device_ops ata_slave_block_device_ops;

void ata_pci_init(struct pci_dev *dev);

#endif
