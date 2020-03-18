/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_CONTAINER_OF_H__
#define __INCLUDE_UAPI_PROTURA_CONTAINER_OF_H__

#include <protura/stddef.h>

/* We only use the fancy version that requires `typeof()` if we're using gcc */
#ifdef __GNUC__
# define container_of(ptr, type, member) ({ \
         const __typeof__(((type *)0)->member) *__mptr = (ptr); \
         (type *)((char *)__mptr - __koffsetof(type, member)); })
#else
# define container_of(ptr, type, member) \
        ((type *)((char *)ptr - __koffsetof(type, member)))
#endif

#endif
