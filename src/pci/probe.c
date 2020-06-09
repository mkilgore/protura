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
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/ide.h>
#include <protura/drivers/ide_dma.h>
#include <protura/drivers/e1000.h>
#include <protura/drivers/rtl.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>

#include "internal.h"

static const struct pci_driver pci_drivers[] = {
#ifdef CONFIG_IDE_DMA_SUPPORT
    {
        .name = "Intel PIIX3 IDE DMA",
        .vendor = PCI_VENDOR_ID_INTEL,
        .device = PCI_DEVICE_ID_82371SB_PIIX3_IDE,
        .device_init = ide_dma_device_init,
    },
#endif
#ifdef CONFIG_NET_RTL8139_DRIVER
    {
        .name = "RealTek RTL8139 Fast Ethernet",
        .vendor = PCI_VENDOR_ID_REALTEK,
        .device = PCI_DEVICE_ID_RTL8139_NET,
        .device_init = rtl_device_init,
    },
#endif
#ifdef CONFIG_NET_E1000_DRIVER
    {
        .name = "Intel E1000 Fast Ethernet",
        .vendor = PCI_VENDOR_ID_INTEL,
        .device = PCI_DEVICE_ID_E1000_NET,
        .device_init = e1000_device_init,
    },
#endif
    {
        .name = NULL,
        .vendor = 0,
        .device = 0,
    }
};

list_head_t pci_dev_list = LIST_HEAD_INIT(pci_dev_list);

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

#define PCI_SUBCLASS_BRIDGE_PCI 0x04

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
    return pci_config_read_uint8(dev, PCI_REG_CLASS);
}

static uint32_t pci_get_subclass(struct pci_dev *dev)
{
    return pci_config_read_uint8(dev, PCI_REG_SUBCLASS);
}

static uint32_t pci_get_procif(struct pci_dev *dev)
{
    return pci_config_read_uint8(dev, PCI_REG_PROG_IF);
}

static uint32_t pci_get_revision(struct pci_dev *dev)
{
    return pci_config_read_uint8(dev, PCI_REG_REVISION_ID);
}

static uint32_t pci_get_header_type(struct pci_dev *dev)
{
    return pci_config_read_uint8(dev, PCI_REG_HEADER_TYPE);
}

static uint32_t pci_get_secondary_bus(struct pci_dev *dev)
{
    return pci_config_read_uint8(dev, PCI_REG_SECONDARY_BUS);
}

void pci_get_class_name(uint8_t class, uint8_t subclass, const char **class_name, const char **subclass_name)
{
    *class_name = NULL;
    *subclass_name = NULL;

    if (class < ARRAY_SIZE(pci_class_names) && class > 0) {
        if (pci_class_names[class]) {
            *class_name = pci_class_names[class];

            if (pci_class_device_names[class])
                if (pci_class_device_names[class][subclass])
                    *subclass_name = pci_class_device_names[class][subclass];
        }
    }
}

static void pci_enumerate_bus(int bus);

/* Add's a device to the main list of devices.
 *
 * Note that we keep this list sorted by bus, slot, and func number.
 */
static void pci_add_dev_entry(struct pci_dev_entry *new_entry)
{
    struct pci_dev_entry *ent;

    list_foreach_entry(&pci_dev_list, ent, pci_dev_node) {
        if (new_entry->info.id.bus < ent->info.id.bus
            || (new_entry->info.id.bus == ent->info.id.bus && new_entry->info.id.slot < ent->info.id.slot)
            || (new_entry->info.id.bus == ent->info.id.bus && new_entry->info.id.slot == ent->info.id.slot && new_entry->info.id.func < ent->info.id.func)) {
            list_add_tail(&ent->pci_dev_node, &new_entry->pci_dev_node);
            return;
        }
    }

    list_add_tail(&pci_dev_list, &new_entry->pci_dev_node);
}

static void pci_dev_info_populate(struct pci_dev_info *info)
{
    pci_get_device_vendor(&info->id, &info->vendor, &info->device);

    if (info->vendor == 0xFFFF)
        return;

    info->header_type = pci_get_header_type(&info->id);
    info->class = pci_get_class(&info->id);
    info->subclass = pci_get_subclass(&info->id);
    info->procif = pci_get_procif(&info->id);
    info->revision = pci_get_revision(&info->id);
}

static void pci_check_dev(struct pci_dev_info *dev)
{
    struct pci_dev_entry *entry = kzalloc(sizeof(*entry), PAL_KERNEL);
    const char *cla = NULL, *sub = NULL;

    list_node_init(&entry->pci_dev_node);
    entry->info = *dev;

    kp(KP_NORMAL, "PCI %d:%d:%d - 0x%04x:0x%04x\n", dev->id.bus, dev->id.slot, dev->id.func, entry->info.vendor, entry->info.device);

    pci_get_class_name(entry->info.class, entry->info.subclass, &cla, &sub);

    if (cla && sub)
        kp(KP_NORMAL, "  - %s, %s\n", cla, sub);
    else if (cla)
        kp(KP_NORMAL, "  - %s\n", cla);

    pci_add_dev_entry(entry);

    kp(KP_NORMAL, " header is bridge: %d, class: %d, subclass: %d, secondary bus: %d, primary bus: %d\n", dev->header_type, dev->class, dev->subclass, pci_get_secondary_bus(&dev->id), pci_config_read_uint8(&dev->id, PCI_REG_PRIMARY_BUS));

    if (dev->header_type & PCI_HEADER_IS_BRIDGE) {
        int new_bus = pci_get_secondary_bus(&dev->id);

        pci_enumerate_bus(new_bus);
    }
}

static void pci_check_slot(int bus, int slot)
{
    struct pci_dev_info info = {
        .id.bus = bus,
        .id.slot = slot,
    };

    pci_dev_info_populate(&info);

    if (info.vendor == 0xFFFF)
        return;

    pci_check_dev(&info);

    if (info.header_type & PCI_HEADER_IS_MULTIFUNCTION) {
        for (info.id.func = 1; info.id.func < 8; info.id.func++) {
            pci_dev_info_populate(&info);

            if (info.vendor == 0xFFFF)
                continue;

            pci_check_dev(&info);
        }
    }
}

static void pci_enumerate_bus(int bus)
{
    int slot;

    for (slot = 0; slot < 32; slot++)
        pci_check_slot(bus, slot);
}

static void enum_pci(void)
{
    pci_enumerate_bus(0);
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
        pci_load_device(&entry->info.id, entry->info.vendor, entry->info.device);
}

void pci_init(void)
{
    enum_pci();
    load_pci_devices();
}

size_t pci_bar_size(struct pci_dev *dev, uint8_t bar_reg)
{
    uint32_t bar = pci_config_read_uint32(dev, bar_reg);
    size_t size;

    pci_config_write_uint32(dev, bar_reg, 0xFFFFFFFF);
    size = pci_config_read_uint32(dev, bar_reg);
    pci_config_write_uint32(dev, bar_reg, bar);

    return (~size) + 1;
}

