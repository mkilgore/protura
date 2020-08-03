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

void ata_pci_init(struct pci_dev *dev);

#endif
