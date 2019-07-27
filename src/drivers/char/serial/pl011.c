/*
 * Copyright (C) 2018 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/basic_printf.h>

#include <arch/spinlock.h>
#include <arch/asm.h>
#include <protura/char_buf.h>
// #include <protura/fs/char.h>
#include <protura/drivers/pl011.h>

#include "pl011_regs.h"

enum pl011_regs {
	PL011_REG_DR,
	PL011_REG_ST_DMAWM,
	PL011_REG_ST_TIMEOUT,
	PL011_REG_FR,
	PL011_REG_LCRH_RX,
	PL011_REG_LCRH_TX,
	PL011_REG_IBRD,
	PL011_REG_FBRD,
	PL011_REG_CR,
	PL011_REG_IFLS,
	PL011_REG_IMSC,
	PL011_REG_RIS,
	PL011_REG_MIS,
	PL011_REG_ICR,
	PL011_REG_DMACR,

	PL011_REG_SIZE,
};


static uint16_t pl011_reg_offsets[PL011_REG_SIZE] = {
	[PL011_REG_DR] = UART01x_DR,
	[PL011_REG_FR] = UART01x_FR,
	[PL011_REG_LCRH_RX] = UART011_LCRH,
	[PL011_REG_LCRH_TX] = UART011_LCRH,
	[PL011_REG_IBRD] = UART011_IBRD,
	[PL011_REG_FBRD] = UART011_FBRD,
	[PL011_REG_CR] = UART011_CR,
	[PL011_REG_IFLS] = UART011_IFLS,
	[PL011_REG_IMSC] = UART011_IMSC,
	[PL011_REG_RIS] = UART011_RIS,
	[PL011_REG_MIS] = UART011_MIS,
	[PL011_REG_ICR] = UART011_ICR,
	[PL011_REG_DMACR] = UART011_DMACR,
};

struct pl011_config {
    io_t io_base;
    int baud;
    int irq;

    int fifosize;
};

struct pl011 {
    struct pl011_config conf;

    int clock_rate;
    unsigned int exists :1;
    unsigned int initialized :1;
    char buffer[CONFIG_COM_BUFSZ];

    spinlock_t buf_lock;
    struct char_buf buf;
    //struct wakeup_list watch_list;
};

static struct pl011 pl011_array[CONFIG_PL011_MAX] = {
    [0] = {
        .conf = {
            .io_base = 0x1000,
            .baud = 38400,
            .irq = 57,
            .fifosize = 16,
        },
        .clock_rate = 3000000,
        .exists = 1,
        .buf_lock = SPINLOCK_INIT("pl011"),
        .buf.buffer = pl011_array[0].buffer,
        .buf.len = sizeof(pl011_array[0].buffer),
    },
};

static uint32_t pl011_reg_read(struct pl011_config *pl011, int reg)
{
    return inl(pl011->io_base + pl011_reg_offsets[reg]);
}

static void pl011_reg_write(struct pl011_config *pl011, int reg, uint32_t val)
{
    outl(pl011->io_base + pl011_reg_offsets[reg], val);
}

static int pl011_write_ready(struct pl011 *pl011)
{
    return (pl011_reg_read(&pl011->conf, PL011_REG_FR) & UART01x_FR_TXFF) == 0;
}

static uint32_t div_round_closest(uint32_t val, uint32_t divisor)
{
    return (val + divisor / 2) / (divisor);
}

static int pl011_init_driver(struct pl011 *pl011)
{
    /* Disable the PL011 */
    pl011_reg_write(&pl011->conf, PL011_REG_CR, 0);

    /* Clear interrupts */
    pl011_reg_write(&pl011->conf, PL011_REG_ICR, 0x7FF);

    uint32_t quotent = (pl011->clock_rate / (pl011->conf.baud * 16)) - 1;

    /* This math places the fractional part in the bottom 6 bits, divisor in top 16 */
    if (pl011->conf.baud > pl011->clock_rate / 16)
        quotent = div_round_closest(pl011->clock_rate * 8, pl011->conf.baud);
    else
        quotent = div_round_closest(pl011->clock_rate * 4, pl011->conf.baud);

    pl011_reg_write(&pl011->conf, PL011_REG_IBRD, quotent >> 6);
    pl011_reg_write(&pl011->conf, PL011_REG_FBRD, quotent & 0x3F);

    uint16_t lcr = UART01x_LCRH_WLEN_8 | UART01x_LCRH_FEN;

    if (pl011->conf.fifosize > 1)
        lcr |= UART01x_LCRH_FEN;

    pl011_reg_write(&pl011->conf, PL011_REG_LCRH_RX, lcr);

    pl011_reg_write(&pl011->conf, PL011_REG_CR, UART011_CR_RXE | UART011_CR_TXE | UART01x_CR_UARTEN);

    pl011->initialized = 1;

    return 0;
}

void pl011_init(void)
{
    struct pl011 *pl011 = pl011_array;

    if (!pl011->initialized)
        pl011_init_driver(pl011);

    return ;
}

void pl011_tty_init(void)
{

}

int pl011_init_early(void)
{
    return pl011_init_driver(pl011_array);
}

struct printf_backbone_pl011 {
    struct printf_backbone backbone;
    int pl011_id;
};

static void pl011_putchar(struct printf_backbone *b, char ch)
{
    struct printf_backbone_pl011 *pl011_backbone = container_of(b, struct printf_backbone_pl011, backbone);
    struct pl011 *pl011 = &pl011_array[pl011_backbone->pl011_id];

    while (!pl011_write_ready(pl011))
        ;

    pl011_reg_write(&pl011->conf, PL011_REG_DR, ch);
}

static void pl011_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        pl011_putchar(b, s[i]);
}

void pl011_printfv(const char *fmt, va_list lst)
{
    struct printf_backbone_pl011 pl011 = {
        .backbone = {
            .putchar = pl011_putchar,
            .putnstr = pl011_putnstr,
        },
        .pl011_id = 0,
    };

    basic_printfv(&pl011.backbone, fmt, lst);
}

