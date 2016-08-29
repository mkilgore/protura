/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

/* Driver for the PC UART COM port */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/signal.h>
#include <protura/kassert.h>
#include <protura/wait.h>
#include <protura/basic_printf.h>

#include <arch/spinlock.h>
#include <protura/drivers/term.h>
#include <arch/drivers/keyboard.h>
#include <arch/asm.h>
#include <arch/drivers/pic8259.h>
#include <arch/idt.h>
#include <protura/fs/char.h>
#include <protura/drivers/com.h>

#define COM_CLOCK 115200

static int com_init_was_early = 0;

enum {
    COM1,
    COM2,
};

struct com_port {
    io_t ioport;
    int baud;
    int irq;

    void (*irq_handler) (struct irq_frame *);

    int exists :1;

    char buffer[CONFIG_COM_BUFSZ];

    spinlock_t buf_lock;
    struct char_buf buf;
    size_t buf_len;
    struct wakeup_list watch_list;
};

static struct com_port com_ports[];

static void com_int_handler(struct com_port *com)
{
    char b;

    using_spinlock(&com->buf_lock) {
        do {
            b = inb(com->ioport + UART_RX);

            char_buf_write_char(&com->buf, b);
            com->buf_len++;
            kp(KP_TRACE, "Buf sz: %d, buf_len: %d\n", (int)sizeof(com->buffer), com->buf_len);
            wakeup_list_wakeup(&com->watch_list);
        } while (inb(com->ioport + UART_LSR) & UART_LSR_DR);
    }
}

static void com1_int_handler(struct irq_frame *frame)
{
    com_int_handler(com_ports + COM1);
}

static void com2_int_handler(struct irq_frame *frame)
{
    com_int_handler(com_ports + COM2);
}

static struct com_port com_ports[] = {
    [COM1] = {
        .ioport = 0x3F8,
        .baud = 38400,
        .irq = PIC8259_IRQ0 + 4,
        .irq_handler = com1_int_handler,
        .exists = 1,
        .buf_lock = SPINLOCK_INIT("com1"),
        .buf.buffer = com_ports[COM1].buffer,
        .buf.len = sizeof(com_ports[COM1].buffer),
    },
    [COM2] = {
        .ioport = 0x2F8,
        .baud = 38400,
        .irq = PIC8259_IRQ0 + 3,
        .irq_handler = com2_int_handler,
        .exists = 1,
        .buf_lock = SPINLOCK_INIT("com2"),
        .buf.buffer = com_ports[COM2].buffer,
        .buf.len = sizeof(com_ports[COM2].buffer),
    },
};

static int com_file_read(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    int cur_pos = 0;
    int com_id = DEV_MINOR(filp->inode->dev_no);
    struct task *current = cpu_get_local()->current;
    struct com_port *com = com_ports + com_id;

    if (com_id > ARRAY_SIZE(com_ports))
        return -ENODEV;

    if (!com->exists)
        return -ENODEV;

    if (!vbuf)
        return -EFAULT;

    kp(KP_TRACE, "Registering %d for COM%d\n", current->pid, com_id);
    wakeup_list_add(&com->watch_list, current);

  sleep_again:
    sleep_intr {
        using_spinlock(&com->buf_lock) {
            while ((cur_pos != len) && (com->buf_len > 0)) {
                buf[cur_pos] = char_buf_read_char(&com->buf);
                cur_pos++;
                com->buf_len--;
                kp(KP_TRACE, "%d: Reading from COM\n", current->pid);
            }
        }

        /* Sleep if there was no data in the buffer */
        if (!cur_pos) {
            scheduler_task_yield();
            if (has_pending_signal(current)) {
                wakeup_list_remove(&com->watch_list, current);
                return -ERESTARTSYS;
            }
            goto sleep_again;
        }
    }

    wakeup_list_remove(&com->watch_list, current);
    kp(KP_TRACE, "Unregistering %d for COM%d\n", current->pid, com_id);

    return cur_pos;
}

static int com_file_write(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    int com_id = DEV_MINOR(filp->inode->dev_no);
    struct com_port *com = com_ports + com_id;

    if (com_id > ARRAY_SIZE(com_ports))
        return -ENODEV;

    if (!com->exists)
        return -ENODEV;

    using_spinlock(&com->buf_lock) {
        int i;
        for (i = 0; i < len; i++)
            outb(com->ioport + UART_TX, buf[i]);
    }

    return len;
}

struct file_ops com_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = com_file_read,
    .write = com_file_write,
    .lseek = NULL,
    .readdir = NULL,
};

static void com_init_ports(void)
{
    int i;
    irq_flags_t irq_flags;

    irq_flags = irq_save();
    irq_disable();

    for (i = 0; i < ARRAY_SIZE(com_ports); i++) {
        if (!com_ports[i].exists)
            continue;

        kp(KP_DEBUG, "Programming COM%d\n", i);

        outb(com_ports[i].ioport + UART_IER, 0x00);
        outb(com_ports[i].ioport + UART_LCR, UART_LCR_DLAB);
        outb(com_ports[i].ioport + UART_DLL, (COM_CLOCK / com_ports[i].baud) & 0xFF);
        outb(com_ports[i].ioport + UART_DLM, (COM_CLOCK / com_ports[i].baud) >> 8);
        outb(com_ports[i].ioport + UART_LCR, UART_LCR_WLEN8);
        outb(com_ports[i].ioport + UART_FCR, UART_FCR_TRIGGER_14
                | UART_FCR_CLEAR_XMIT
                | UART_FCR_CLEAR_RCVR
                | UART_FCR_ENABLE_FIFO);
        outb(com_ports[i].ioport + 4, 0x0B);

        if (inb(com_ports[i].ioport + UART_LSR) == 0xFF) {
            kp(KP_DEBUG, "COM%d not found\n", i);
            com_ports[i].exists = 0;
            continue;
        }

        kp(KP_DEBUG, "Registering COM%d IRQ\n", i);

        irq_register_callback(com_ports[i].irq, com_ports[i].irq_handler, "com", IRQ_INTERRUPT);
        pic8259_enable_irq(com_ports[i].irq);

        outb(com_ports[i].ioport + UART_IER, UART_IER_RDI);

        /* Clear interrupt registers after we enable them. */
        inb(com_ports[i].ioport + UART_LSR);
        inb(com_ports[i].ioport + UART_RX);
        inb(com_ports[i].ioport + UART_IIR);
        inb(com_ports[i].ioport + UART_MSR);
    }

    irq_restore(irq_flags);
}

void com_init(void)
{
    if (!com_init_was_early)
        com_init_ports();
}

void com_init_early(void)
{
    com_init_ports();
    com_init_was_early = 1;
}

struct printf_backbone_com {
    struct printf_backbone backbone;
    int com_id;
};

/* Note that com1_printf is only used for kernel log output - Thus, we don't
 * take the lock for this. */
static void com_putchar(struct printf_backbone *b, char ch)
{
    struct printf_backbone_com *com = container_of(b, struct printf_backbone_com, backbone);
    outb(com_ports[com->com_id].ioport + UART_TX, ch);
}

static void com_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    int i;
    struct printf_backbone_com *com = container_of(b, struct printf_backbone_com, backbone);

    for (i = 0; i < len; i++)
        outb(com_ports[com->com_id].ioport + UART_TX, s[i]);
}

void com1_printfv(const char *fmt, va_list lst)
{
    struct printf_backbone_com com = {
        .backbone = {
            .putchar = com_putchar,
            .putnstr = com_putnstr,
        },
        .com_id = COM1,
    };

    basic_printfv(&com.backbone, fmt, lst);
}

void com2_printfv(const char *fmt, va_list lst)
{
    struct printf_backbone_com com = {
        .backbone = {
            .putchar = com_putchar,
            .putnstr = com_putnstr,
        },
        .com_id = COM2,
    };

    basic_printfv(&com.backbone, fmt, lst);
}

