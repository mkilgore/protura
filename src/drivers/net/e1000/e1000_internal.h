#ifndef SRC_DRIVERS_NET_E1000_INTERNAL_H
#define SRC_DRIVERS_NET_E1000_INTERNAL_H

#include <protura/compiler.h>
#include <protura/drivers/pci.h>
#include <protura/net.h>

#define REG_CTRL     0x0000
#define REG_STATUS   0x0008
#define REG_EEPROM   0x0014
#define REG_CTRL_EXT 0x0018
#define REG_ICR      0x00C0
#define REG_IMS      0x00D0
#define REG_RCTRL    0x0100
#define REG_RDBAL    0x2800
#define REG_RDBAH    0x2804
#define REG_RDLEN    0x2808
#define REG_RDH      0x2810
#define REG_RDT      0x2818

#define REG_TCTRL 0x0400
#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH   0x3810
#define REG_TDT   0x3818

#define REG_RDTR        0x2820 /* RX Delay Timer Register */
#define REG_RXDCTL      0x3828 /* RX Descriptor Control */
#define REG_RADV        0x282C /* RX Int. Absolute Delay Timer */
#define REG_RSRPD       0x2C00 /* RX Small Packet Detect Interrupt */

#define REG_TIPG        0x0410      /* Transmit Inter Packet Gap */
#define ECTRL_SLU       0x40        /* set link up */
#define ECTRL_FD        0x00000001  /* Full duplex.0=half; 1=full */

#define RCTL_EN            (1 << 1)    /* Receiver Enable */
#define RCTL_SBP           (1 << 2)    /* Store Bad Packets */
#define RCTL_UPE           (1 << 3)    /* Unicast Promiscuous Enabled */
#define RCTL_MPE           (1 << 4)    /* Multicast Promiscuous Enabled */
#define RCTL_LPE           (1 << 5)    /* Long Packet Reception Enable */
#define RCTL_LBM_NONE      (0 << 6)    /* No Loopback */
#define RCTL_LBM_PHY       (3 << 6)    /* PHY or external SerDesc loopback */
#define RTCL_RDMTS_HALF    (0 << 8)    /* Free Buffer Threshold is 1/2 of RDLEN */
#define RTCL_RDMTS_QUARTER (1 << 8)    /* Free Buffer Threshold is 1/4 of RDLEN */
#define RTCL_RDMTS_EIGHTH  (2 << 8)    /* Free Buffer Threshold is 1/8 of RDLEN */
#define RCTL_MO_36         (0 << 12)   /* Multicast Offset - bits 47:36 */
#define RCTL_MO_35         (1 << 12)   /* Multicast Offset - bits 46:35 */
#define RCTL_MO_34         (2 << 12)   /* Multicast Offset - bits 45:34 */
#define RCTL_MO_32         (3 << 12)   /* Multicast Offset - bits 43:32 */
#define RCTL_BAM           (1 << 15)   /* Broadcast Accept Mode */
#define RCTL_VFE           (1 << 18)   /* VLAN Filter Enable */
#define RCTL_CFIEN         (1 << 19)   /* Canonical Form Indicator Enable */
#define RCTL_CFI           (1 << 20)   /* Canonical Form Indicator Bit Value */
#define RCTL_DPF           (1 << 22)   /* Discard Pause Frames */
#define RCTL_PMCF          (1 << 23)   /* Pass MAC Control Frames */
#define RCTL_SECRC         (1 << 26)   /* Strip Ethernet CRC */

/* Buffer Sizes */
#define RCTL_BSIZE_256   (3 << 16)
#define RCTL_BSIZE_512   (2 << 16)
#define RCTL_BSIZE_1024  (1 << 16)
#define RCTL_BSIZE_2048  (0 << 16)
#define RCTL_BSIZE_4096  ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192  ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384 ((1 << 16) | (1 << 25))

/* Transmit Command */
#define TXD_CMD_EOP  (1 << 0)    /* End of Packet */
#define TXD_CMD_IFCS (1 << 1)    /* Insert FCS */
#define TXD_CMD_IC   (1 << 2)    /* Insert Checksum */
#define TXD_CMD_RS   (1 << 3)    /* Report Status */
#define TXD_CMD_RPS  (1 << 4)    /* Report Packet Sent */
#define TXD_CMD_VLE  (1 << 6)    /* VLAN Packet Enable */
#define TXD_CMD_IDE  (1 << 7)    /* Interrupt Delay Enable */


/* TCTL Register */
#define TCTL_EN         (1 << 1)    /* Transmit Enable */
#define TCTL_PSP        (1 << 3)    /* Pad Short Packets */
#define TCTL_CT_SHIFT   4           /* Collision Threshold */
#define TCTL_COLD_SHIFT 12          /* Collision Distance */
#define TCTL_SWXOFF     (1 << 22)   /* Software XOFF Transmission */
#define TCTL_RTLC       (1 << 24)   /* Re-transmit on Late Collision */

#define TSTA_DD (1 << 0)    /* Descriptor Done */
#define TSTA_EC (1 << 1)    /* Excess Collisions */
#define TSTA_LC (1 << 2)    /* Late Collision */
#define LSTA_TU (1 << 3)    /* Transmit Underrun */

/* IMS flags */
#define IMS_TXDW   0x00000001 /* Transmit desc written back */
#define IMS_LSC    0x00000004 /* Link Status Change */
#define IMS_RXSEQ  0x00000008 /* rx sequence error */
#define IMS_RXDMT0 0x00000010 /* rx desc min. threshold */
#define IMS_RXT0   0x00000080 /* rx timer intr */

/* ICR flags */
#define ICR_TXDW          0x00000001	/* Transmit desc written back */
#define ICR_TXQE          0x00000002	/* Transmit Queue empty */
#define ICR_LSC           0x00000004	/* Link Status Change */
#define ICR_RXSEQ         0x00000008	/* rx sequence error */
#define ICR_RXDMT0        0x00000010	/* rx desc min. threshold (0) */
#define ICR_RXO           0x00000040	/* rx overrun */
#define ICR_RXT0          0x00000080	/* rx timer intr (ring 0) */
#define ICR_MDAC          0x00000200	/* MDIO access complete */
#define ICR_RXCFG         0x00000400	/* RX /c/ ordered set */
#define ICR_GPI_EN0       0x00000800	/* GP Int 0 */
#define ICR_GPI_EN1       0x00001000	/* GP Int 1 */
#define ICR_GPI_EN2       0x00002000	/* GP Int 2 */
#define ICR_GPI_EN3       0x00004000	/* GP Int 3 */
#define ICR_TXD_LOW       0x00008000
#define ICR_SRPD          0x00010000
#define ICR_ACK           0x00020000	/* Receive Ack frame */
#define ICR_MNG           0x00040000	/* Manageability event */
#define ICR_DOCK          0x00080000	/* Dock/Undock */
#define ICR_INT_ASSERTED  0x80000000	/* If this bit asserted, the driver should claim the interrupt */
#define ICR_RXD_FIFO_PAR0 0x00100000	/* queue 0 Rx descriptor FIFO parity error */
#define ICR_TXD_FIFO_PAR0 0x00200000	/* queue 0 Tx descriptor FIFO parity error */
#define ICR_HOST_ARB_PAR  0x00400000	/* host arb read buffer parity error */
#define ICR_PB_PAR        0x00800000	/* packet buffer parity error */
#define ICR_RXD_FIFO_PAR1 0x01000000	/* queue 1 Rx descriptor FIFO parity error */
#define ICR_TXD_FIFO_PAR1 0x02000000	/* queue 1 Tx descriptor FIFO parity error */
#define ICR_ALL_PARITY    0x03F00000	/* all parity error bits */
#define ICR_DSW           0x00000020	/* FW changed the status of DISSW bit in the FWSM */
#define ICR_PHYINT        0x00001000	/* LAN connected device generates an interrupt */
#define ICR_EPRST         0x00100000	/* ME hardware reset occurs */

/* RX desc status flags */
#define RXD_STAT_DD    0x01   /* Descriptor Done */
#define RXD_STAT_EOP   0x02   /* End of Packet */
#define RXD_STAT_IXSM  0x04   /* Ignore checksum */
#define RXD_STAT_VP    0x08   /* IEEE VLAN Packet */
#define RXD_STAT_UDPCS 0x10   /* UDP xsum calculated */
#define RXD_STAT_TCPCS 0x20   /* TCP xsum calculated */
#define RXD_STAT_IPCS  0x40   /* IP xsum calculated */
#define RXD_STAT_PIF   0x80   /* passed in-exact filter */
#define RXD_STAT_IPIDV 0x200  /* IP identification valid */
#define RXD_STAT_UDPV  0x400  /* Valid UDP checksum */
#define RXD_STAT_ACK   0x8000 /* ACK Packet indication */

/* RX desc err flags */
#define RXD_ERR_CE   0x01 /* CRC Error */
#define RXD_ERR_SE   0x02 /* Symbol Error */
#define RXD_ERR_SEQ  0x04 /* Sequence Error */
#define RXD_ERR_CXE  0x10 /* Carrier Extension Error */
#define RXD_ERR_TCPE 0x20 /* TCP/UDP Checksum Error */
#define RXD_ERR_IPE  0x40 /* IP Checksum Error */
#define RXD_ERR_RXE  0x80 /* Rx Data Error */

/* RX desc special flags */
#define RXD_SPC_VLAN_MASK 0x0FFF /* VLAN ID is in lower 12 bits */
#define RXD_SPC_PRI_MASK  0xE000 /* Priority is in upper 3 bits */
#define RXD_SPC_PRI_SHIFT 13
#define RXD_SPC_CFI_MASK  0x1000 /* CFI is bit 12 */
#define RXD_SPC_CFI_SHIFT 12

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __packed;

struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __packed;

struct net_interface_e1000 {
    struct net_interface net;

    io_t io_base;
    void *mem_base;
    struct pci_dev dev;

    uint8_t has_eeprom :1;

    spinlock_t rx_lock;
    struct e1000_rx_desc *rx_descs;
    int cur_rx; /* RDT = cur_rx - 1 */

    spinlock_t tx_lock;
    list_head_t tx_packet_queue;
    struct e1000_tx_desc *tx_descs;
    int last_clear; /* TDH - but updated manually */
    int next_tx; /* TDT = next_tx */
};

#endif
