/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_OHCI_H
#define INCLUDE_DRIVERS_OHCI_H

void ohci_pci_init(struct pci_dev *dev);

#endif
