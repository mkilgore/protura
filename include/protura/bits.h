/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_BITS
#define INCLUDE_PROTURA_BITS

#include <protura/limits.h>
#include <arch/bitops.h>

#define HEX(n) 0x ## n ## UL
#define BINARY(x) (((x & 0x0000000FUL)? 1: 0) \
        + ((x & 0x000000F0UL)? 2: 0) \
        + ((x & 0x00000F00UL)? 4: 0) \
        + ((x & 0x0000F000UL)? 8: 0) \
        + ((x & 0x000F0000UL)? 16: 0) \
        + ((x & 0x00F00000UL)? 32: 0) \
        + ((x & 0x0F000000UL)? 64: 0) \
        + ((x & 0xF0000000UL)? 128: 0))

#define b8(bin) ((unsigned char)BINARY(HEX(bin)))
#define b16(binhigh, binlow) (((unsigned short)b8(binhigh) << 8) + b8(binlow))
#define b32(binhigh, binmid1, binmid2, binlow) (((unsigned int)b8(binhigh) << 24) + (b8(binmid1) << 16) + (b8(binmid2) << 8) + b8(binlow))

/* Sets the bit referred to by 'flag'. */
#define F(flag) (1 << (flag))

#endif
