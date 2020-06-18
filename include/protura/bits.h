/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_BITS
#define INCLUDE_PROTURA_BITS

#include <protura/stddef.h>
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
#define __F1(flag) (1 << (flag))
#define __F2(...)  (__F1(CAR(__VA_ARGS__)) | __F1(CDR(__VA_ARGS__)))
#define __F3(...)  (__F1(CAR(__VA_ARGS__)) | __F2(CDR(__VA_ARGS__)))
#define __F4(...)  (__F1(CAR(__VA_ARGS__)) | __F3(CDR(__VA_ARGS__)))
#define __F5(...)  (__F1(CAR(__VA_ARGS__)) | __F4(CDR(__VA_ARGS__)))
#define __F6(...)  (__F1(CAR(__VA_ARGS__)) | __F5(CDR(__VA_ARGS__)))
#define __F7(...)  (__F1(CAR(__VA_ARGS__)) | __F6(CDR(__VA_ARGS__)))
#define __F8(...)  (__F1(CAR(__VA_ARGS__)) | __F7(CDR(__VA_ARGS__)))
#define __F9(...)  (__F1(CAR(__VA_ARGS__)) | __F8(CDR(__VA_ARGS__)))
#define __F10(...) (__F1(CAR(__VA_ARGS__)) | __F9(CDR(__VA_ARGS__)))
#define __F11(...) (__F1(CAR(__VA_ARGS__)) | __F10(CDR(__VA_ARGS__)))
#define __F12(...) (__F1(CAR(__VA_ARGS__)) | __F11(CDR(__VA_ARGS__)))
#define __F13(...) (__F1(CAR(__VA_ARGS__)) | __F12(CDR(__VA_ARGS__)))
#define __F14(...) (__F1(CAR(__VA_ARGS__)) | __F13(CDR(__VA_ARGS__)))
#define __F15(...) (__F1(CAR(__VA_ARGS__)) | __F14(CDR(__VA_ARGS__)))
#define __F16(...) (__F1(CAR(__VA_ARGS__)) | __F15(CDR(__VA_ARGS__)))
#define __F17(...) (__F1(CAR(__VA_ARGS__)) | __F16(CDR(__VA_ARGS__)))
#define __F18(...) (__F1(CAR(__VA_ARGS__)) | __F17(CDR(__VA_ARGS__)))
#define __F19(...) (__F1(CAR(__VA_ARGS__)) | __F18(CDR(__VA_ARGS__)))
#define __F20(...) (__F1(CAR(__VA_ARGS__)) | __F19(CDR(__VA_ARGS__)))

#define __FINNER2(cnt) \
    __F ## cnt

#define __FINNER(cnt) \
    __FINNER2(cnt)

#define F(...) (__FINNER(COUNT_ARGS(__VA_ARGS__)) (__VA_ARGS__))

#endif
