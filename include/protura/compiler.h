/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_COMPILER_H
#define INCLUDE_COMPILER_H

#define __must_check __attribute__((warn_unused_result))

#define barrier() __asm__ __volatile__("": : :"memory")

#define noinline __attribute_((noinline))
#define __always_inline    inline __attribute__((always_inline))

#define __packed __attribute__((packed))

#define __printf(str, args)  __attribute__((format(printf, str, args)))

#define __align(size) __attribute__((__aligned__(size)))


#endif

