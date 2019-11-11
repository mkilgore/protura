#ifndef SRC_DRIVERS_NET_RTL8139_INTERNAL_H
#define SRC_DRIVERS_NET_RTL8139_INTERNAL_H

#include <protura/drivers/pci.h>
#include <protura/net.h>
#include <protura/spinlock.h>

/* MAC Address */
#define REG_MAC0  0x00
#define REG_MAC1  0x01
#define REG_MAC2  0x02
#define REG_MAC3  0x03
#define REG_MAC4  0x04
#define REG_MAC5  0x05

/* Multicast */
#define REG_MAR0  0x08
#define REG_MAR1  0x09
#define REG_MAR2  0x0A
#define REG_MAR3  0x0B
#define REG_MAR4  0x0C
#define REG_MAR5  0x0D
#define REG_MAR6  0x0E
#define REG_MAR7  0x0F

/* Transmit Status for Descriptors 0-3 */
#define REG_TSD0 0x10
#define REG_TSD1 0x14
#define REG_TSD2 0x18
#define REG_TSD3 0x1C
#define REG_TSD(x) (0x10 + (x) * 4)

/* Transmit Start Address for Descriptors 0-3 */
#define REG_TSAD0 0x20
#define REG_TSAD1 0x24
#define REG_TSAD2 0x28
#define REG_TSAD3 0x2C
#define REG_TSAD(x) (0x20 + (x) * 4)

#define REG_RBSTART 0x30 /* Rx Buffer Start Address */
#define REG_ERBCR   0x34 /* Early Rx Byte Count */
#define REG_ERSR    0x36 /* Early Rx Status Register */
#define REG_CR      0x37 /* Command Register */
#define REG_CAPR    0x38 /* Current Address of Packet Read */
#define REG_CBR     0x3A /* Current Buffer Address */
#define REG_IMR     0x3C /* Interrupt Mask Register */
#define REG_ISR     0x3E /* Interrupt Status Register */
#define REG_TCR     0x40 /* Transmit Configuration Register */
#define REG_RCR     0x44 /* Receive Configuration Register */
#define REG_TCRT    0x48 /* Timer Count Register */
#define REG_MPC     0x4C /* Missed Packet Counter */

/* #define REG_9346CR 0x50 */

#define REG_CONFIG0 0x51 /* Config Register 0 */
#define REG_CONFIG1 0x52 /* Config Register 1 */

#define REG_TIMERINT 0x54 /* Timer Interrupt Register */
#define REG_MSR      0x58 /* Media Status Register */
#define REG_CONFIG3  0x59 /* Config Register 3 */
#define REG_CONFIG4  0x5A /* Config Register 4 */
#define REG_MULINT   0x5C /* Multiple Interrupt Select */
#define REG_RERID    0x5E /* PCI Revision ID */
#define REG_TASD     0x60 /* Transmit Status for all Descriptors */
#define REG_BMCR     0x62 /* Base Mode Control Register */
#define REG_BMSR     0x64 /* Base Mode Status Register */
#define REG_ANAR     0x66 /* Auto-negotiation Advertisement Register */
#define REG_ANLPAR   0x68 /* Auto-negotiation Link Partner Register */
#define REG_ANER     0x6A /* Auto-negotiation Expansion Register */
#define REG_DIS      0x6C /* Disconnect Counter */
#define REG_FCSC     0x6E /* False Carrier Sense Counter */
#define REG_NWAYTR   0x70 /* N-way Test Register */
#define REG_REC      0x72 /* RX_ER Counter */
#define REG_CSCR     0x74 /* CS Configuration Register */

#define REG_PHY1_PARM 0x78 /* PHY Parameter 1 */
#define REG_TW_PARM   0x7C /* Twister Parameter */
#define REG_PHY2_PARM 0x80 /* PHY Parameter 2 */

/* Flags for the REG_CR Register */
#define REG_CR_BUFE 0x01 /* RX Buffer Empty */
#define REG_CR_TE   0x04 /* TX Enable */
#define REG_CR_RE   0x08 /* RX Enable */
#define REG_CR_RST  0x10 /* Software Reset */

/* When the below bits are set in the IMR, those interrupts are enabled */
#define REG_IMR_ROK     0x0001 /* Receive OK Interrupt */
#define REG_IMR_RER     0x0002 /* Receive ERR Interrupt */
#define REG_IMR_TOK     0x0004 /* Transmit OK Interrupt */
#define REG_IMR_TER     0x0008 /* Transmit ERR Interrupt */
#define REG_IMR_RXOVW   0x0010 /* RX Buffer Overflow Interrupt */
#define REG_IMR_PUN     0x0020 /* Packet Underrun or Link Change Interrupt */
#define REG_IMR_FOVW    0x0040 /* RX FIFO Ofervlow Interrupt */
#define REG_IMR_LENCHG  0x2000 /* Cable Length Change Interrupt */
#define REG_IMR_TIMEOUT 0x4000 /* Timeout Interrupt */
#define REG_IMR_SERR    0x8000 /* System Error interrupt */

/* See IMR definitions - The ISR indicates which of those interrupts has occured
 * when an interrupt happens */
#define REG_ISR_ROK     0x0001 /* Receive OK Interrupt */
#define REG_ISR_RER     0x0002 /* Receive ERR Interrupt */
#define REG_ISR_TOK     0x0004 /* Transmit OK Interrupt */
#define REG_ISR_TER     0x0008 /* Transmit ERR Interrupt */
#define REG_ISR_RXOVW   0x0010 /* RX Buffer Overflow Interrupt */
#define REG_ISR_PUN     0x0020 /* Packet Underrun or Link Change Interrupt */
#define REG_ISR_FOVW    0x0040 /* RX FIFO Ofervlow Interrupt */
#define REG_ISR_LENCHG  0x2000 /* Cable Length Change Interrupt */
#define REG_ISR_TIMEOUT 0x4000 /* Timeout Interrupt */
#define REG_ISR_SERR    0x8000 /* System Error interrupt */

/* Transmit Configuration Register - TCR */
#define REG_TCR_CLRABT    0x00000001 /* Clear Abort */
#define REG_TCR_TXRR      0x000000F0 /* TX Retry Count */
#define REG_TCR_MXDMA     0x00000700 /* Max DMa Burst Size per TX DMA Burst */
#define REG_TCR_CRC       0x00010000 /* Append CRC */
#define REG_TCR_LBK       0x00060000 /* Lookback Test */
#define REG_TCR_HWVERID_B 0x00C00000 /* Hardware Version ID B */
#define REG_TCR_IFG       0x03000000 /* Interframe Gap Time */
#define REG_TCR_HWVERID_A 0x7C000000 /* Hardware Version ID A */

/* Receive Configuration Register - RCR */
#define REG_RCR_AAP       0x00000001 /* Accept All Packets */
#define REG_RCR_APM       0x00000002 /* Accept Physical Match Packets */
#define REG_RCR_AM        0x00000004 /* Accept Multicast Packets */
#define REG_RCR_AB        0x00000008 /* Accept All Broadcast Packets */
#define REG_RCR_AR        0x00000010 /* Accept Runt Packets */
#define REG_RCR_AER       0x00000020 /* Accept Error Packets */
#define REG_RCR_WRAP      0x00000080 /* Wrap Rx Buffer */
#define REG_RCR_MXDMA     0x00000700 /* Max DMA Burst per Rx DMA Burst */
#define REG_RCR_RBLEN     0x00001800 /* Rx Buffer Length */
#define REG_RCR_RXFTH     0x0000E000 /* Rx FIFO Threshold */
#define REG_RCR_RER8      0x00010000 /* Error packet length 8 */
#define REG_RCR_MULERINT  0x00020000 /* Multiple Early Interrupt Select */
#define REG_RCR_ERTH      0x0F000000 /* Early Rx Threshold Bits */

/* Settings for RBLEN field */
#define RBLEN_8 0 /* 8k + 16 bytes */
#define RBLEN_16 1 /* 16k + 16 bytes */
#define RBLEN_32 2 /* 32k + 16 bytes */
#define RBLEN_64 3 /* 64k + 16 bytes */

/* Settings for MXDMA fields */
#define MXDMA_DMA_16 0 /* 16-byte DMA */
#define MXDMA_DMA_32 1 /* 32-byte DMA */
#define MXDMA_DMA_64 2 /* 64-byte DMA */
#define MXDMA_DMA_128 3 /* 128-byte DMA */
#define MXDMA_DMA_256 4 /* 256-byte DMA */
#define MXDMA_DMA_512 5 /* 512-byte DMA */
#define MXDMA_DMA_1024 6 /* 1024-byte DMA */
#define MXDMA_DMA_UNLIMIT /* Unlimited DMA */


/* Transmit Status Register - TSD0-3 */
#define REG_TSD_SIZE_MASK 0x00001FFF /* Descriptor Size */
#define REG_TSD_OWN       0x00002000 /* Set when TX DMA is complete */
#define REG_TSD_TUN       0x00004000 /* Transmit FIFO Underrun */
#define REG_TSD_TOK       0x00008000 /* Transmit OK */
/* Incomplete */

/* Early RX Status Register - ERSR */
#define REG_ERSR_EROK   0x01 /* Early RX OK */
#define REG_ERSR_EROVW  0x02 /* Early RX OverWrite */
#define REG_ERSR_ERBAD  0x04 /* Early RX Bad Packet */
#define REG_ERSR_ERGOOD 0x08 /* Early RX Good Packet */


struct net_interface_rtl {
    struct net_interface net;

    io_t io_base;
    char mac[6];

    struct page *rx_buffer;
    int rx_cur_offset;

    struct page *tx_buffer[4];
    int tx_cur_buffer;

    struct pci_dev dev;
};

struct rtl_rx_packet {
    uint16_t status;
    uint16_t len;
    char packet[];
};

/* Rx Status Register - in Rx Packet Header */
#define RSR_ROK     0x0001 /* Receive OK */
#define RSR_FAE     0x0002 /* Frame Alignment Error */
#define RSR_CRC     0x0004 /* CRC Error */
#define RSR_LONG    0x0008 /* Packet excedes 4K bytes */
#define RSR_RUNT    0x0010 /* Packet is smaller then 64 bytes */
#define RSR_ISE     0x0020 /* Invalid Symbol Error */
#define RSR_BAR     0x2000 /* Broadcast address */
#define RSR_PAM     0x4000 /* Physical address match */
#define RSR_MAR     0x8000 /* Multicast address */

#endif
