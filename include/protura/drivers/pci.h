#ifndef INCLUDE_PROTURA_DRIVERS_PCI_PCI_H
#define INCLUDE_PROTURA_DRIVERS_PCI_PCI_H

#include <protura/types.h>

struct pci_dev {
    uint8_t bus, slot, func;
};

#define PRpci_dev "%d:%d:%d"
#define Ppci_dev(dev) (dev)->bus, (dev)->slot, (dev)->func

int pci_find_device(uint16_t vendor, uint16_t device, struct pci_dev *dev);

void pci_init(void);

uint32_t pci_config_read_uint32(struct pci_dev *dev, uint8_t regno);
uint16_t pci_config_read_uint16(struct pci_dev *dev, uint8_t regno);
uint8_t  pci_config_read_uint8(struct pci_dev *dev, uint8_t regno);

void pci_config_write_uint32(struct pci_dev *dev, uint8_t regno, uint32_t value);
void pci_config_write_uint16(struct pci_dev *dev, uint8_t regno, uint16_t value);
void pci_config_write_uint8(struct pci_dev *dev, uint8_t regno, uint8_t value);

#define PCI_REG_BAR(x) (0x10 + ((x) * 4))
#define PCI_REG_INTERRUPT_LINE (0x3C)

#define PCI_VENDOR_ID   0x00
#define PCI_DEVICE_ID   0x02
#define PCI_COMMAND     0x04
#define PCI_STATUS      0x06
#define PCI_REVISION_ID 0x08

#endif
