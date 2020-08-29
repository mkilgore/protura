#ifndef SRC_DRIVERS_BLOCK_ATA_H
#define SRC_DRIVERS_BLOCK_ATA_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/block/bcache.h>
#include <arch/spinlock.h>
#include <protura/drivers/pci.h>

/* structure returned by HDIO_GET_IDENTITY, as per ANSI ATA2 rev.2f spec.
 *
 * Format from the Linux Kernel source code. */
struct ata_identify_format {
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

#define ATA_CAPABILITY_DMA 0x01

#define PRD_MAX 10

struct ata_dma_prd {
    uint32_t addr;
    uint32_t bcnt;
} __packed __align(0x08);

enum {
    ATA_PORT_DATA = 0,
    ATA_PORT_FEAT_ERR = 1,
    ATA_PORT_SECTOR_CNT = 2,
    ATA_PORT_LBA_LOW_8 = 3,
    ATA_PORT_LBA_MID_8= 4,
    ATA_PORT_LBA_HIGH_8 = 5,
    ATA_PORT_DRIVE_HEAD = 6,
    ATA_PORT_COMMAND_STATUS = 7,

    ATA_STATUS_BUSY = (1 << 7),
    ATA_STATUS_READY = (1 << 6),
    ATA_STATUS_DRIVE_FAULT = (1 << 5),
    ATA_STATUS_DATA_REQUEST = (1 << 3),
    ATA_STATUS_DATA_CORRECT = (1 << 2),
    ATA_STATUS_ERROR = (1 << 0),

    /* This is not a real part of the status, but we report this if the status check hits the timeout */
    ATA_STATUS_TIMEOUT = (1 << 8),

    ATA_CTL_STOP_INT = (1 << 1),
    ATA_CTL_RESET = (1 << 2),

    ATA_DH_SHOULD_BE_SET = (1 << 5) | (1 << 7),
    ATA_DH_LBA = (1 << 6),

    /* Set this bit to use the slave ATA drive instead of master */
    ATA_DH_SLAVE = (1 << 4),

    ATA_COMMAND_PIO_LBA28_READ = 0x20,
    ATA_COMMAND_PIO_LBA28_WRITE = 0x30,

    ATA_COMMAND_DMA_LBA28_READ = 0xC8,
    ATA_COMMAND_DMA_LBA28_WRITE = 0xCA,

    ATA_COMMAND_CACHE_FLUSH = 0xE7,
    ATA_COMMAND_IDENTIFY = 0xEC,

    ATA_SECTOR_SIZE = 512,

    ATA_DMA_IO_CMD = 0,
    ATA_DMA_IO_STAT = 2,
    ATA_DMA_IO_PRDT = 4,

    ATA_DMA_CMD_SSBM = (1 << 0), /* Start bus mastering */
    ATA_DMA_CMD_RWCON = (1 << 3), /* If set, we're doing a read */
};

struct ata_drive {
    spinlock_t lock;

    struct block *current;
    size_t current_sector_offset;
    int sectors_left;

    list_head_t block_queue_master;
    list_head_t block_queue_slave;

    struct ata_dma_prd prdt[PRD_MAX];

    io_t io_base;
    io_t ctrl_io_base;
    io_t dma_base;
    int drive_irq;

    uint8_t has_master :1;
    uint8_t has_slave  :1;
    uint8_t use_dma    :1;

    uint32_t master_sectors;
    uint32_t slave_sectors;

    int refs;
};

#endif
