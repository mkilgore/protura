/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_IDE_H
#define INCLUDE_DRIVERS_IDE_H

#include <fs/block.h>

void ide_init(void);

/* Block should be locked before syncing */
void ide_sync_block(struct block_device *, struct block *);

extern struct block_device_ops ide_block_device_ops;

#endif
