#ifndef SRC_USB_OHCI_H
#define SRC_USB_OHCI_H

#include <protura/types.h>
#include <protura/usb/usb.h>

/*
 * The following register definitions are from the Linux Kernel source (GPLv2)
 *
 * The naming style matches the names used in the OHCI spec for entries
 * specified by the spec.
 */

struct ohci_hcca {
    uint32_t InterruptTable[32];
    uint16_t FrameNumber;
    uint16_t Pad1;
    uint32_t DoneHead;
    uint8_t reserved[116];
    uint8_t padding[4];
} __align(256) __packed;

struct ohci_regs {
    uint32_t HcRevision;
    uint32_t HcControl;
    uint32_t HcCommandStatus;
    uint32_t HcInterruptStatus;
    uint32_t HcInterruptEnable;
    uint32_t HcInterruptDisable;

    uint32_t HcHCCA;
    uint32_t HcPeriodCurrentED;
    uint32_t HcControlHeadED;
    uint32_t HcControlCurrentED;
    uint32_t HcBulkHeadED;
    uint32_t HcBulkCurrentED;
    uint32_t HcDoneHead;

    uint32_t HcFmInterval;
    uint32_t HcFmRemaining;
    uint32_t HcFmNumber;
    uint32_t HcPeriodicStart;
    uint32_t HcLSThreshold;

    uint32_t HcRhDescriptorA;
    uint32_t HcRhDescriptorB;
    uint32_t HcRhStatus;
    uint32_t HcRhPortStatus[15];
} __align(32) __packed;

struct ohci_ed {
    struct {
        uint32_t info;    /* endpoint config bitmap */

        uint32_t TailP;
        uint32_t HeadP;

        uint32_t NextED;
    } hw __packed;

    struct ohci_td *dummy;
    list_node_t ed_node;

    enum usb_pipe_type type;
} __align(16);

struct ohci_td {
    struct {
        uint32_t info;

        uint32_t CBP;
        uint32_t NextTD;
        uint32_t BE;

        uint16_t hwPSW[8];
    } hw __packed;

    struct ohci_ed *ed;

    int urb_index;
    struct urb *urb;

    void *buffer;
    size_t buffer_len;

    /* Used when reversing the list of tds to process */
    struct ohci_td *rev_list_next;
} __align(32);

/*
 * TD bits
 */

/* hwINFO bits for both general and iso tds: */
#define OHCI_TD_CC           0xf0000000            /* condition code */
#define OHCI_TD_CC_GET(td_p) ((td_p >>28) & 0x0f)

#define OHCI_TD_DI        0x00E00000            /* frames before interrupt */
#define OHCI_TD_DI_SET(X) (((X) & 0x07)<< 21)

/* hwINFO bits for general tds: */
#define OHCI_TD_EC       0x0C000000    /* error count */
#define OHCI_TD_T        0x03000000    /* data toggle state */
#define OHCI_TD_T_DATA0  0x02000000    /* DATA0 */
#define OHCI_TD_T_DATA1  0x03000000    /* DATA1 */
#define OHCI_TD_T_TOGGLE 0x00000000    /* uses ED_C */
#define OHCI_TD_DP       0x00180000    /* direction/pid */
#define OHCI_TD_DP_SETUP 0x00000000    /* SETUP pid */
#define OHCI_TD_DP_IN    0x00100000    /* IN pid */
#define OHCI_TD_DP_OUT   0x00080000    /* OUT pid */
/* 0x00180000 rsvd */
#define OHCI_TD_R        0x00040000    /* round: short packets OK? */

/*
 * ED bits
 */
/* info bits defined by the hardware */
#define OHCI_ED_ISO       (1 << 15)
#define OHCI_ED_SKIP      (1 << 14)
#define OHCI_ED_LOWSPEED  (1 << 13)
#define OHCI_ED_OUT       (0x01 << 11)
#define OHCI_ED_IN        (0x02 << 11)

#define OHCI_ED_C         (0x02)    /* toggle carry */
#define OHCI_ED_H         (0x01)    /* halted */

/*
 * HcControl register masks
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
 * HcCommandStatus register masks
 */
#define OHCI_HCR (1 << 0)  /* host controller reset */
#define OHCI_CLF (1 << 1)  /* control list filled */
#define OHCI_BLF (1 << 2)  /* bulk list filled */
#define OHCI_OCR (1 << 3)  /* ownership change request */
#define OHCI_SOC (3 << 16) /* scheduling overrun count */

/*
 * masks used with interrupt registers:
 * HcInterruptStatus 
 * HcInterruptEnable
 * HcInterruptDisable
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

/* HcRhPortStatus[i] bits */
#define OHCI_RH_PS_CCS  0x00000001 /* current connect status */
#define OHCI_RH_PS_PES  0x00000002 /* port enable status*/
#define OHCI_RH_PS_PSS  0x00000004 /* port suspend status */
#define OHCI_RH_PS_POCI 0x00000008 /* port over current indicator */
#define OHCI_RH_PS_PRS  0x00000010 /* port reset status */
#define OHCI_RH_PS_PPS  0x00000100 /* port power status */
#define OHCI_RH_PS_LSDA 0x00000200 /* low speed device attached */
#define OHCI_RH_PS_CSC  0x00010000 /* connect status change */
#define OHCI_RH_PS_PESC 0x00020000 /* port enable status change */
#define OHCI_RH_PS_PSSC 0x00040000 /* port suspend status change */
#define OHCI_RH_PS_OCIC 0x00080000 /* over current indicator change */
#define OHCI_RH_PS_PRSC 0x00100000 /* port reset status change */

/* HcRhStatus bits */
#define OHCI_RH_HS_LPS  0x00000001 /* local power status */
#define OHCI_RH_HS_OCI  0x00000002 /* over current indicator */
#define OHCI_RH_HS_DRWE 0x00008000 /* device remote wakeup enable */
#define OHCI_RH_HS_LPSC 0x00010000 /* local power status change */
#define OHCI_RH_HS_OCIC 0x00020000 /* over current indicator change */
#define OHCI_RH_HS_CRWE 0x80000000 /* clear remote wakeup enable */

/* HcRhDescriptorB masks */
#define OHCI_RH_B_DR   0x0000ffff /* device removable flags */
#define OHCI_RH_B_PPCM 0xffff0000 /* port power control mask */

/* HcRhDescriptorA masks */
#define OHCI_RH_A_NDP    (0xff << 0)  /* number of downstream ports */
#define OHCI_RH_A_PSM    (1 << 8)     /* power switching mode */
#define OHCI_RH_A_NPS    (1 << 9)     /* no power switching */
#define OHCI_RH_A_DT     (1 << 10)    /* device type (mbz) */
#define OHCI_RH_A_OCPM   (1 << 11)    /* over current protection mode */
#define OHCI_RH_A_NOCP   (1 << 12)    /* no over current protection */
#define OHCI_RH_A_POTPGT (0xff << 24) /* power on to power good time */

struct ohci_bus {
    struct usb_bus bus;

    struct ohci_hcca *hcca;
    struct ohci_regs *regs;
    int port_count;

    /* Hotplug lock protects usages of ed0 for assigning addresses via the
     * special 0 endpoint */
    mutex_t hotplug_lock;
    struct ohci_ed *ed0;

    uint32_t ctrl_copy;

    list_head_t ed_list;
};

struct ohci_usb_device {
    struct usb_device device;

    struct ohci_ed *ineds[16];
    struct ohci_ed *outeds[16];
};

struct ohci_urb_priv {
    int total_tds;
    int tds_left;

    int next_td;
    struct ohci_td *tds[];
};

#define OHCI_BUS_INIT(b) \
    { \
        .bus = USB_BUS_INIT((b).bus), \
        .ed_list = LIST_HEAD_INIT((b).ed_list), \
        .hotplug_lock = MUTEX_INIT((b).hotplug_lock), \
    }

static inline void ohci_bus_init(struct ohci_bus *bus)
{
    *bus = (struct ohci_bus)OHCI_BUS_INIT(*bus);
}

#endif
