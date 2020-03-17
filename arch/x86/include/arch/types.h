/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_TYPES_H
#define INCLUDE_ARCH_TYPES_H

#include <uapi/arch/types.h>

#include <protura/compiler.h>

typedef __kuint8_t uint8_t;
typedef __kint8_t   int8_t;

typedef __kuint16_t uint16_t;
typedef __kint16_t   int16_t;

typedef __kuint32_t uint32_t;
typedef __kint32_t   int32_t;

typedef __kuint64_t uint64_t;
typedef __kint64_t   int64_t;

typedef __ksize_t size_t;

typedef __kuintptr_t uintptr_t;
typedef __kintptr_t   intptr_t;

typedef __kva_t va_t;
typedef __kpa_t pa_t;

typedef __kpn_t pn_t;
typedef __kio_t io_t;

#define va_make(va) ((va_t)(va))
#define pa_make(pa) ((pa_t)(pa))

#define va_val(va) ((void *)(va))
#define pa_val(pa) ((uintptr_t)(pa))

#define v_to_p(va) (pa_t)(V2P((uintptr_t)(va)))
#define p_to_v(pa) (va_t)(P2V((uintptr_t)(pa)))

#endif

