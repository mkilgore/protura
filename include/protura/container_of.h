/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_CONTAINER_OF_H
#define INCLUDE_PROTURA_CONTAINER_OF_H

#ifndef container_of
# define container_of(ptr, type, member) ({ \
         const typeof(((type *)0)->member) *__mptr = (ptr); \
         (type *)((char *)__mptr - offsetof(type, member)); })
#endif

#endif
