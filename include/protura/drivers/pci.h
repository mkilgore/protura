#ifndef INCLUDE_PROTURA_DRIVERS_PCI_PCI_H
#define INCLUDE_PROTURA_DRIVERS_PCI_PCI_H

#include <protura/types.h>
#include <protura/fs/file.h>

struct pci_dev {
    uint8_t bus, slot, func;
};

struct pci_driver {
    const char *name;
    uint16_t vendor, device;
    void (*device_init) (struct pci_dev *);
};

#define PRpci_dev "%d:%d:%d"
#define Ppci_dev(dev) (dev)->bus, (dev)->slot, (dev)->func

void pci_init(void);

uint32_t pci_config_read_uint32(struct pci_dev *dev, uint8_t regno);
uint16_t pci_config_read_uint16(struct pci_dev *dev, uint8_t regno);
uint8_t  pci_config_read_uint8(struct pci_dev *dev, uint8_t regno);

void pci_config_write_uint32(struct pci_dev *dev, uint8_t regno, uint32_t value);
void pci_config_write_uint16(struct pci_dev *dev, uint8_t regno, uint16_t value);
void pci_config_write_uint8(struct pci_dev *dev, uint8_t regno, uint8_t value);

#define PCI_REG_BAR(x) (0x10 + ((x) * 4))
#define PCI_REG_INTERRUPT_LINE (0x3C)

#define PCI_REG_VENDOR_ID   0x00
#define PCI_REG_DEVICE_ID   0x02
#define PCI_REG_COMMAND     0x04
#define PCI_REG_STATUS      0x06
#define PCI_REG_REVISION_ID 0x08
#define PCI_REG_PROG_IF     0x09
#define PCI_REG_SUBCLASS    0x0A
#define PCI_REG_CLASS       0x0B
#define PCI_REG_HEADER_TYPE 0x0E


/*
 * Bits for the PCI_COMMAND config register
 * Bits 7, and 11 to 15 are reserved.
 */
#define PCI_COMMAND_IO_SPACE        (1 << 0) /* I/O Space */
#define PCI_COMMAND_MEM_SPACE       (1 << 1) /* Memory Space */
#define PCI_COMMAND_BUS_MASTER      (1 << 2) /* Bus Master */
#define PCI_COMMAND_SPEC_CYCLES     (1 << 3) /* Special Cycles */
#define PCI_COMMAND_MEM_ERR         (1 << 4) /* Memory Write and Invalidate Enable */
#define PCI_COMMAND_VGA_PAL         (1 << 5) /* VGA Palette Snoop */
#define PCI_COMMAND_PERR_RESPONSE   (1 << 6) /* Parity Error Response */
#define PCI_COMMAND_SERR_ENABLE     (1 << 8) /* SERR Driver Enable */
#define PCI_COMMAND_FAST_B2B_ENABLE (1 << 9) /* Fast Back-to-Back Enable */
#define PCI_COMMAND_INT_DISABLE     (1 << 10 /* Interrupt Disable */

#define PCI_BAR_IO 0x00000001

size_t pci_bar_size(struct pci_dev *dev, uint8_t bar_reg);

extern const struct file_ops pci_file_ops;

#endif
