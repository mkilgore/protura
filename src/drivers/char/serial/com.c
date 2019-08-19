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
#include <protura/wait.h>
#include <protura/basic_printf.h>
#include <protura/work.h>

#include <arch/spinlock.h>
#include <protura/drivers/term.h>
#include <arch/drivers/keyboard.h>
#include <arch/asm.h>
#include <arch/drivers/pic8259.h>
#include <arch/idt.h>
#include <protura/fs/char.h>
#include <protura/drivers/tty.h>
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

    int exists :1;

    char buffer[CONFIG_COM_BUFSZ];

    spinlock_t buf_lock;
    struct char_buf buf;
    list_head_t work_list;
};

static struct com_port com_ports[];

static int com_write_ready(struct com_port *com)
{
    return inb(com->ioport + UART_LSR) & UART_LSR_THRE;
}

static void com_int_handler(struct irq_frame *frame, void *param)
{
    char b;
    struct com_port *com = param;
    struct work *work;

    using_spinlock(&com->buf_lock) {
        do {
            b = inb(com->ioport + UART_RX);

            char_buf_write_char(&com->buf, b);

            list_foreach_entry(&com->work_list, work, wakeup_entry)
                work_schedule(work);
        } while (inb(com->ioport + UART_LSR) & UART_LSR_DR);
    }
}

static struct com_port com_ports[] = {
    [COM1] = {
        .ioport = 0x3F8,
        .baud = 38400,
        .irq = PIC8259_IRQ0 + 4,
        .exists = 1,
        .buf_lock = SPINLOCK_INIT("com1"),
        .buf.buffer = com_ports[COM1].buffer,
        .buf.len = sizeof(com_ports[COM1].buffer),
        .work_list = LIST_HEAD_INIT(com_ports[COM1].work_list),
    },
    [COM2] = {
        .ioport = 0x2F8,
        .baud = 38400,
        .irq = PIC8259_IRQ0 + 3,
        .exists = 1,
        .buf_lock = SPINLOCK_INIT("com2"),
        .buf.buffer = com_ports[COM2].buffer,
        .buf.len = sizeof(com_ports[COM2].buffer),
        .work_list = LIST_HEAD_INIT(com_ports[COM2].work_list),
    },
};

static int com_file_read(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    int ret = 0;
    size_t cur_pos = 0;
    size_t com_id = DEV_MINOR(filp->inode->dev_no);
    struct task *current = cpu_get_local()->current;
    struct com_port *com = com_ports + com_id;

    if (com_id > ARRAY_SIZE(com_ports))
        return -ENODEV;

    if (!com->exists)
        return -ENODEV;

    if (!vbuf)
        return -EFAULT;

    struct work com_work = WORK_INIT_TASK(com_work, current);

    kp(KP_TRACE, "Registering %d for COM%d\n", current->pid, com_id);
    using_spinlock(&com->buf_lock)
        list_add_tail(&com->work_list, &com_work.wakeup_entry);

    scoped_spinlock(&com->buf_lock) {
        while (1) {
            while ((cur_pos != len) && (com->buf.buf_len > 0)) {
                buf[cur_pos] = char_buf_read_char(&com->buf);
                cur_pos++;
                kp(KP_TRACE, "%d: Reading from COM\n", current->pid);
            }

            if (cur_pos == len)
                break;

            ret = sleep_event_intr_spinlock(com->buf.buf_len > 0, &com->buf_lock);
            if (ret)
                goto cleanup;
        }
    }

    ret = cur_pos;

  cleanup:
    using_spinlock(&com->buf_lock)
        list_del(&com_work.wakeup_entry);

    kp(KP_TRACE, "Unregistering %d for COM%d\n", current->pid, com_id);

    return ret;
}

static int com_file_write(struct file *filp, const void *vbuf, size_t len)
{
    const char *buf = vbuf;
    size_t com_id = DEV_MINOR(filp->inode->dev_no);
    struct com_port *com = com_ports + com_id;

    if (com_id > ARRAY_SIZE(com_ports))
        return -ENODEV;

    if (!com->exists)
        return -ENODEV;

    using_spinlock(&com->buf_lock) {
        size_t i;
        for (i = 0; i < len; i++) {
            while (!com_write_ready(com))
                ;
            outb(com->ioport + UART_TX, buf[i]);
        }
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
    size_t i;
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

        irq_register_callback(com_ports[i].irq, com_int_handler, "com", IRQ_INTERRUPT, com_ports + i);
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

static int com_tty_read(struct tty *tty, char *buf, size_t len)
{
    struct com_port *com = com_ports + tty->device_no;
    size_t cur_pos = 0;

    using_spinlock(&com->buf_lock) {
        while ((cur_pos != len) && (com->buf.buf_len > 0)) {
            buf[cur_pos] = char_buf_read_char(&com->buf);
            cur_pos++;
        }
    }

    return cur_pos;
}

static int com_tty_write(struct tty *tty, const char *buf, size_t len)
{
    struct com_port *com = com_ports + tty->device_no;

    using_spinlock(&com->buf_lock) {
        size_t i;
        for (i = 0; i < len; i++) {
            while (!com_write_ready(com))
                ;
            outb(com->ioport + UART_TX, buf[i]);
        }
    }

    return len;
}

static int com_tty_has_chars(struct tty *tty)
{
    struct com_port *com = com_ports + tty->device_no;
    int ret;

    using_spinlock(&com->buf_lock)
        ret = com->buf.buf_len;

    return ret;
}

static void com_tty_register_for_wakeups(struct tty *tty)
{
    struct com_port *com = com_ports + tty->device_no;

    using_spinlock(&com->buf_lock)
        list_add_tail(&com->work_list, &tty->work.wakeup_entry);
}

static struct tty_ops ops = {
    .read = com_tty_read,
    .write = com_tty_write,
    .has_chars = com_tty_has_chars,
    .register_for_wakeups = com_tty_register_for_wakeups,
};

static struct tty_driver driver = {
    .name = "ttyS",
    .minor_start = 2,
    .minor_end = 3,
    .ops = &ops,
};

void com_tty_init(void)
{
    kp(KP_TRACE, "COM TTY INIT\n");
    tty_driver_register(&driver);
}

void com_init(void)
{
    if (!com_init_was_early)
        com_init_ports();
}

int com_init_early(void)
{
    com_init_ports();
    com_init_was_early = 1;

    return 0;
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
    size_t i;
    struct printf_backbone_com *com = container_of(b, struct printf_backbone_com, backbone);

    for (i = 0; i < len; i++) {
        while (!com_write_ready(com_ports + com->com_id))
            ;
        outb(com_ports[com->com_id].ioport + UART_TX, s[i]);
    }
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

