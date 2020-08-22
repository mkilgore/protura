#ifndef SRC_USB_OHCI_H
#define SRC_USB_OHCI_H

#include <protura/types.h>
#include <protura/usb/usb.h>

/* The following register definitions are from the Linux Kernel source (GPLv2) */

struct ohci_hcca {
#define NUM_INTS 32
    uint32_t int_table[NUM_INTS];   /* periodic schedule */
    uint16_t frame_no;               /* current frame number */
    uint16_t pad1;                   /* set to 0 on each frame_no change */
    uint32_t done_head;              /* info returned for an interrupt */
    uint8_t reserved_for_hc[116];
    uint8_t padding[4];
} __align(256) __packed;

struct ohci_regs {
    uint32_t revision;
    uint32_t control;
    uint32_t command_status;
    uint32_t interrupt_status;
    uint32_t interrupt_enable;
    uint32_t interrupt_disable;

    uint32_t hcca;
    uint32_t period_current_ed;
    uint32_t control_head_ed;
    uint32_t control_current_ed;
    uint32_t bulk_head_ed;
    uint32_t bulk_current_ed;
    uint32_t done_head;

    uint32_t fm_interval;
    uint32_t fm_remaining;
    uint32_t fm_number;
    uint32_t periodic_start;
    uint32_t ls_threshold;

    uint32_t roothub_descriptor_a;
    uint32_t roothub_descriptor_b;
    uint32_t roothub_status;
    uint32_t roothub_port_status[15];
} __align(32) __packed;

/*
 * control register masks
 */
#define OHCI_CTRL_CBSR (3 << 0)  /* control/bulk service ratio */
#define OHCI_CTRL_PLE  (1 << 2)  /* periodic list enable */
#define OHCI_CTRL_IE   (1 << 3)  /* isochronous enable */
#define OHCI_CTRL_CLE  (1 << 4)  /* control list enable */
#define OHCI_CTRL_BLE  (1 << 5)  /* bulk list enable */
#define OHCI_CTRL_HCFS (3 << 6)  /* host controller functional state */
#define OHCI_CTRL_IR   (1 << 8)  /* interrupt routing */
#define OHCI_CTRL_RWC  (1 << 9)  /* remote wakeup connected */
#define OHCI_CTRL_RWE  (1 << 10) /* remote wakeup enable */

/* pre-shifted values for HCFS */
#define OHCI_USB_RESET   (0 << 6)
#define OHCI_USB_RESUME  (1 << 6)
#define OHCI_USB_OPER    (2 << 6)
#define OHCI_USB_SUSPEND (3 << 6)

/*
 * command_status register masks
 */
#define OHCI_HCR (1 << 0)  /* host controller reset */
#define OHCI_CLF (1 << 1)  /* control list filled */
#define OHCI_BLF (1 << 2)  /* bulk list filled */
#define OHCI_OCR (1 << 3)  /* ownership change request */
#define OHCI_SOC (3 << 16) /* scheduling overrun count */

/*
 * masks used with interrupt registers:
 * interrupt_status
 * interrupt_enable
 * interrupt_disable
 */
#define OHCI_INTR_SO   (1 << 0)  /* scheduling overrun */
#define OHCI_INTR_WDH  (1 << 1)  /* writeback of done_head */
#define OHCI_INTR_SF   (1 << 2)  /* start frame */
#define OHCI_INTR_RD   (1 << 3)  /* resume detect */
#define OHCI_INTR_UE   (1 << 4)  /* unrecoverable error */
#define OHCI_INTR_FNO  (1 << 5)  /* frame number overflow */
#define OHCI_INTR_RHSC (1 << 6)  /* root hub status change */
#define OHCI_INTR_OC   (1 << 30) /* ownership change */
#define OHCI_INTR_MIE  (1 << 31) /* master interrupt enable */

/* roothub_port_status [i] bits */
#define RH_PS_CCS  0x00000001 /* current connect status */
#define RH_PS_PES  0x00000002 /* port enable status*/
#define RH_PS_PSS  0x00000004 /* port suspend status */
#define RH_PS_POCI 0x00000008 /* port over current indicator */
#define RH_PS_PRS  0x00000010 /* port reset status */
#define RH_PS_PPS  0x00000100 /* port power status */
#define RH_PS_LSDA 0x00000200 /* low speed device attached */
#define RH_PS_CSC  0x00010000 /* connect status change */
#define RH_PS_PESC 0x00020000 /* port enable status change */
#define RH_PS_PSSC 0x00040000 /* port suspend status change */
#define RH_PS_OCIC 0x00080000 /* over current indicator change */
#define RH_PS_PRSC 0x00100000 /* port reset status change */

/* roothub_status bits */
#define RH_HS_LPS  0x00000001 /* local power status */
#define RH_HS_OCI  0x00000002 /* over current indicator */
#define RH_HS_DRWE 0x00008000 /* device remote wakeup enable */
#define RH_HS_LPSC 0x00010000 /* local power status change */
#define RH_HS_OCIC 0x00020000 /* over current indicator change */
#define RH_HS_CRWE 0x80000000 /* clear remote wakeup enable */

/* roothub_descriptor_b masks */
#define RH_B_DR   0x0000ffff /* device removable flags */
#define RH_B_PPCM 0xffff0000 /* port power control mask */

/* roothub_descriptor_a masks */
#define RH_A_NDP    (0xff << 0)  /* number of downstream ports */
#define RH_A_PSM    (1 << 8)     /* power switching mode */
#define RH_A_NPS    (1 << 9)     /* no power switching */
#define RH_A_DT     (1 << 10)    /* device type (mbz) */
#define RH_A_OCPM   (1 << 11)    /* over current protection mode */
#define RH_A_NOCP   (1 << 12)    /* no over current protection */
#define RH_A_POTPGT (0xff << 24) /* power on to power good time */

struct ohci_bus {
    struct usb_bus bus;

    struct ohci_hcca *hcca;
    struct ohci_regs *regs;
    int port_count;
};

#define OHCI_BUS_INIT(b) \
    { \
        .bus = USB_BUS_INIT((b).bus), \
    }

static inline void ohci_bus_init(struct ohci_bus *bus)
{
    *bus = (struct ohci_bus)OHCI_BUS_INIT(*bus);
}

#endif
