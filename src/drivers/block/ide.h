#ifndef SRC_DRIVERS_BLOCK_IDE_H
#define SRC_DRIVERS_BLOCK_IDE_H

#include <protura/types.h>
#include <protura/fs/block.h>
#include <protura/drivers/pci.h>

enum {
    IDE_SECTOR_SIZE = 512,
};

/* structure returned by HDIO_GET_IDENTITY, as per ANSI ATA2 rev.2f spec.
 *
 * Format from the Linux Kernel source code. */
struct ide_identify_format {
    uint16_t config; /* lots of obsolete bit flags */
    uint16_t cyls;/* "physical" cyls */
    uint16_t reserved2; /* reserved (word 2) */
    uint16_t heads;  /* "physical" heads */
    uint16_t track_bytes; /* unformatted bytes per track */
    uint16_t sector_bytes; /* unformatted bytes per sector */
    uint16_t sectors; /* "physical" sectors per track */
    uint16_t vendor0; /* vendor unique */
    uint16_t vendor1; /* vendor unique */
    uint16_t vendor2; /* vendor unique */
    uint8_t serial_no[20]; /* 0 = not_specified */
    uint16_t buf_type;
    uint16_t buf_size; /* 512 byte increments; 0 = not_specified */
    uint16_t ecc_bytes; /* for r/w long cmds; 0 = not_specified */
    uint8_t  fw_rev[8]; /* 0 = not_specified */
    uint8_t  model[40]; /* 0 = not_specified */
    uint8_t  max_multsect; /* 0=not_implemented */
    uint8_t  vendor3; /* vendor unique */
    uint16_t dword_io; /* 0=not_implemented; 1=implemented */
    uint8_t  vendor4; /* vendor unique */
    uint8_t  capability; /* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup*/
    uint16_t reserved50; /* reserved (word 50) */
    uint8_t  vendor5; /* vendor unique */
    uint8_t  tPIO;  /* 0=slow, 1=medium, 2=fast */
    uint8_t  vendor6; /* vendor unique */
    uint8_t  tDMA;  /* 0=slow, 1=medium, 2=fast */
    uint16_t field_valid; /* bits 0:cur_ok 1:eide_ok */
    uint16_t cur_cyls; /* logical cylinders */
    uint16_t cur_heads; /* logical heads */
    uint16_t cur_sectors; /* logical sectors per track */
    uint16_t cur_capacity0; /* logical total sectors on drive */
    uint16_t cur_capacity1; /*  (2 words, misaligned int)     */
    uint8_t  multsect; /* current multiple sector count */
    uint8_t  multsect_valid; /* when (bit0==1) multsect is ok */
    uint32_t lba_capacity; /* total number of sectors */
    uint16_t dma_1word; /* single-word dma info */
    uint16_t dma_mword; /* multiple-word dma info */
    uint16_t eide_pio_modes; /* bits 0:mode3 1:mode4 */
    uint16_t eide_dma_min; /* min mword dma cycle time (ns) */
    uint16_t eide_dma_time; /* recommended mword dma cycle time (ns) */
    uint16_t eide_pio;       /* min cycle time (ns), no IORDY  */
    uint16_t eide_pio_iordy; /* min cycle time (ns), with IORDY */
    uint16_t reserved69; /* reserved (word 69) */
    uint16_t reserved70; /* reserved (word 70) */
} __packed;

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
    int dma_dir;

    struct ide_dma_prd prd_table[PRD_MAX];
};

#ifdef CONFIG_IDE_DMA_SUPPORT
void ide_dma_init(struct ide_dma_info *, struct pci_dev *);

int ide_dma_setup_read(struct ide_dma_info *, struct block *);
int ide_dma_setup_write(struct ide_dma_info *, struct block *);

void ide_dma_start(struct ide_dma_info *);

void ide_dma_abort(struct ide_dma_info *);
int  ide_dma_check(struct ide_dma_info *);
void ide_dma_clear_error(struct ide_dma_info *info);
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
static inline void ide_dma_clear_error(struct ide_dma_info *info) { }

#endif

#endif
