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
#include <protura/kassert.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/ide.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>

static const struct pci_driver pci_drivers[] = {
    {
        .name = "Intel PIIX3 IDE DMA",
        .vendor = PCI_VENDOR_ID_INTEL,
        .device = PCI_DEVICE_ID_82371SB_PIIX3_IDE,
        .device_init = ide_dma_device_init,
    },
    {
        .name = NULL,
        .vendor = 0,
        .device = 0,
    }
};

static list_head_t pci_dev_list = LIST_HEAD_INIT(pci_dev_list);

struct pci_dev_entry {
    struct pci_dev id;
    uint8_t class, subclass, procif, revision;
    uint16_t vendor, device;

    list_node_t pci_dev_node;
};

#define PCI_CLASS_X \
    X(NONE, "No PCI Class"), \
    X(MASS_STORAGE, "Mass Storage Device"), \
    X(NETWORK,      "Network Controller"), \
    X(DISPLAY,      "Display Controller"), \
    X(MULTIMEDIA,   "Multimedia Controller"), \
    X(MEMORY,       "Memory Controller"), \
    X(BRIDGE,       "PCI Bridge Device"), \
    X(SIMPLE_COMM,  "Simple Communications Controller"), \
    X(BASE_SYSTEM,  "Base System Peripherals"), \
    X(INPUT,        "Input Device"), \
    X(DOCKING,      "Docking Station"), \
    X(PROCESSOR,    "Processor"), \
    X(SERIAL_BUS,   "Serial Bus"), \
    X(WIRELESS,     "Wireless Controller"), \
    X(INTELLIGENT_IO, "Intelligent I/O Controller"), \
    X(SATELITE,       "Satelite Communication Controller"), \
    X(ENCRYPT_DECRYPT, "Encryption/Decryption Controller"), \
    X(SIGNAL_PROC,     "Data Acquisition and Signal Processing Controller")

#define PCI_CLASS_STORAGE_X \
    X(SCSI, 0, "SCSI Storage Controller"), \
    X(IDE,  1, "IDE Interface"), \
    X(FLOPPY, 2, "Floppy disk controller"), \
    X(IPI, 3, "IPI bus controller"), \
    X(RAID, 4, "RAID bus controller"), \
    X(ATA, 5, "ATA controller"), \
    X(SATA, 6, "SATA controller"), \
    X(SERIAL_SCSI, 7, "Serial Attached SCSI controller"), \
    X(NON_VOLATILE, 8, "Non-Volatile memory controller"), \
    X(MASS_STORAGE, 80, "Mass storage controller")

enum {
#define X(en, ...) PCI_CLASS_##en
    PCI_CLASS_X,
#undef X
    PCI_CLASS_UNKNOWN = 0xFF,
};

enum {
#define X(en, val, ...) PCI_CLASS_STORAGE_##en = val
    PCI_CLASS_STORAGE_X
#undef X
};

static const char *pci_class_storage_names[] = {
#define X(en, val, name) [PCI_CLASS_STORAGE_##en] = name
    PCI_CLASS_STORAGE_X
#undef X
};

static const char *pci_class_names[] = {
#define X(en, name) [PCI_CLASS_##en] = name
    PCI_CLASS_X,
#undef X
    [PCI_CLASS_UNKNOWN] = "PCI Class Unknown",
};

static const char **pci_class_device_names[PCI_CLASS_UNKNOWN] = {
    [PCI_CLASS_MASS_STORAGE] = pci_class_storage_names,
};

struct pci_addr {
    union {
        uint32_t addr;
        struct {
            uint32_t regno    :8;
            uint32_t funcno   :3;
            uint32_t slotno    :5;
            uint32_t busno    :8;
            uint32_t reserved :7;
            uint32_t enabled :1;
        };
    };
};

#define PCI_IO_CONFIG_ADDRESS (0xCF8)
#define PCI_IO_CONFIG_DATA    (0xCFC)

#define PCI_ADDR_CREATE(dev, reg) \
    { \
        .enabled = 1, \
        .busno = ((dev)->bus), \
        .slotno = ((dev)->slot), \
        .funcno = ((dev)->func), \
        .regno = ((reg) & 0xFC), \
    }

uint32_t pci_config_read_uint32(struct pci_dev *dev, uint8_t regno)
{
    struct pci_addr addr = PCI_ADDR_CREATE(dev, regno);

    outl(PCI_IO_CONFIG_ADDRESS, addr.addr);
    return inl(PCI_IO_CONFIG_DATA);
}

uint16_t pci_config_read_uint16(struct pci_dev *dev, uint8_t regno)
{
    struct pci_addr addr = PCI_ADDR_CREATE(dev, regno);

    outl(PCI_IO_CONFIG_ADDRESS, addr.addr);
    return inw(PCI_IO_CONFIG_DATA + (regno & 3));
}

uint8_t pci_config_read_uint8(struct pci_dev *dev, uint8_t regno)
{
    struct pci_addr addr = PCI_ADDR_CREATE(dev, regno);

    outl(PCI_IO_CONFIG_ADDRESS, addr.addr);
    return inb(PCI_IO_CONFIG_DATA + (regno & 3));
}

void pci_config_write_uint32(struct pci_dev *dev, uint8_t regno, uint32_t value)
{
    struct pci_addr addr = PCI_ADDR_CREATE(dev, regno);

    outl(PCI_IO_CONFIG_ADDRESS, addr.addr);
    outl(PCI_IO_CONFIG_DATA, value);
}

void pci_config_write_uint16(struct pci_dev *dev, uint8_t regno, uint16_t value)
{
    struct pci_addr addr = PCI_ADDR_CREATE(dev, regno);

    outl(PCI_IO_CONFIG_ADDRESS, addr.addr);
    outw(PCI_IO_CONFIG_DATA + (regno & 3), value);
}

void pci_config_write_uint8(struct pci_dev *dev, uint8_t regno, uint8_t value)
{
    struct pci_addr addr = PCI_ADDR_CREATE(dev, regno);

    outl(PCI_IO_CONFIG_ADDRESS, addr.addr);
    outb(PCI_IO_CONFIG_DATA + (regno & 3), value);
}

static void pci_get_device_vendor(struct pci_dev *dev, uint16_t *vendor, uint16_t *device)
{
    uint32_t devven = pci_config_read_uint32(dev, 0);

    *device = (devven >> 16);
    *vendor = (devven & 0xFFFF);
}

static uint32_t pci_get_class(struct pci_dev *dev)
{
    return (pci_config_read_uint32(dev, 8) >> 24) & 0xFF;
}

static uint32_t pci_get_subclass(struct pci_dev *dev)
{
    return (pci_config_read_uint32(dev, 8) >> 16) & 0xFF;
}

static uint32_t pci_get_procif(struct pci_dev *dev)
{
    return (pci_config_read_uint32(dev, 8) >> 8) & 0xFF;
}

static uint32_t pci_get_revision(struct pci_dev *dev)
{
    return pci_config_read_uint32(dev, 8) & 0xFF;
}

static void pci_output_name(uint8_t class, uint8_t subclass)
{
    const char *cla = NULL, *sub = NULL;

    if (class < ARRAY_SIZE(pci_class_names) && class > 0) {
        if (pci_class_names[class]) {
            cla = pci_class_names[class];

            if (pci_class_device_names[class])
                if (pci_class_device_names[class][subclass])
                    sub = pci_class_device_names[class][subclass];
        }
    }

    if (cla && sub) {
        kp(KP_NORMAL, "(%d)%p  - %s, %s\n", class, sub, cla, sub);
    } else if (cla) {
        kp(KP_NORMAL, "  - %s\n", cla);
    }

    return ;
}

static void pci_create_dev(struct pci_dev *dev)
{
    struct pci_dev_entry *entry = kzalloc(sizeof(*entry), PAL_KERNEL);

    list_node_init(&entry->pci_dev_node);
    entry->id = *dev;

    entry->class = pci_get_class(dev);
    entry->subclass = pci_get_subclass(dev);
    entry->procif = pci_get_procif(dev);
    entry->revision = pci_get_revision(dev);
    pci_get_device_vendor(dev, &entry->vendor, &entry->device);

    kp(KP_NORMAL, "PCI %d:%d:%d - 0x%04x:0x%04x\n", dev->bus, dev->slot, dev->func, entry->vendor, entry->device);
    pci_output_name(entry->class, entry->subclass);

    list_add_tail(&pci_dev_list, &entry->pci_dev_node);
}

static void enum_pci(void)
{
    int bus, slot, func;

    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            for (func = 0; func < 8; func++) {
                struct pci_dev addr = { .bus = bus, .slot = slot, .func = func };
                uint16_t ven, dev;
                pci_get_device_vendor(&addr, &ven, &dev);
                if (ven != 0xFFFF)
                    pci_create_dev(&addr);
            }
        }
    }
}

static void pci_load_device(struct pci_dev *dev, uint16_t vendor, uint16_t device)
{
    const struct pci_driver *driver;
    for (driver = pci_drivers; driver->name; driver++)
        if (driver->vendor == vendor && driver->device == device)
            (driver->device_init) (dev);
}

static void load_pci_devices(void)
{
    struct pci_dev_entry *entry;

    list_foreach_entry(&pci_dev_list, entry, pci_dev_node)
        pci_load_device(&entry->id, entry->vendor, entry->device);
}

void pci_init(void)
{
    enum_pci();
    load_pci_devices();
}

