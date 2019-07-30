#ifndef INCLUDE_PROTURA_DRIVERS_IDE_DMA_H
#define INCLUDE_PROTURA_DRIVERS_IDE_DMA_H

#include <protura/types.h>
#include <protura/fs/block.h>
#include <protura/drivers/pci.h>

#define PRD_MAX 10

struct ide_dma_prd {
    uint32_t addr;
    uint32_t bcnt;
} __packed __align(0x08);

struct ide_dma_info {
    struct pci_dev device;
    unsigned int is_enabled :1;

    io_t dma_io_base;
    uint8_t dma_irq;

    struct ide_dma_prd prd_table[PRD_MAX];
};

#ifdef CONFIG_IDE_DMA_SUPPORT
void ide_dma_init(struct ide_dma_info *, struct pci_dev *);

int ide_dma_setup_read(struct ide_dma_info *, struct block *);
int ide_dma_setup_write(struct ide_dma_info *, struct block *);

void ide_dma_start(struct ide_dma_info *);

void ide_dma_abort(struct ide_dma_info *);
int  ide_dma_check(struct ide_dma_info *);

void ide_dma_device_init(struct pci_dev *dev);

#else

static inline void ide_dma_init(struct ide_dma_info *info, struct pci_dev *dev) { }

static inline int ide_dma_setup_read(struct ide_dma_info *info, struct block *blk)
{
    return -ENOTSUP;
}

static inline int ide_dma_setup_write(struct ide_dma_info *info, struct block *blk)
{
    return -ENOTSUP;
}

static inline void ide_dma_start(struct ide_dma_info *info) { }

static inline void ide_dma_abort(struct ide_dma_info *info) { }
static inline int  ide_dma_check(struct ide_dma_info *info)
{
    return -ENOTSUP;
}

#endif

#endif
