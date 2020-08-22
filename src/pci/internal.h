/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_SRC_PCI_INTERNAL_H
#define INCLUDE_SRC_PCI_INTERNAL_H

#include <protura/list.h>
#include <protura/mutex.h>
#include <protura/drivers/pci.h>

struct pci_dev_info {
    struct pci_dev id;
    uint8_t class, subclass, progif, revision, header_type;
    uint16_t vendor, device;
};

struct pci_dev_entry {
    struct pci_dev_info info;

    list_node_t pci_dev_node;
};

/*
 * This is currently readonly after initialization
 */
extern list_head_t pci_dev_list;

void pci_get_class_name(uint8_t class, uint8_t subclass, const char **class_name, const char **subclass_name);

#endif
