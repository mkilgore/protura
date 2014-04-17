/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_COMPILER_H
#define INCLUDE_COMPILER_H

#define __user            __attribute__((noderef, address_space(1)))
#define __kernel          __attribute__((address_space(0)))
#define __safe            __attribute__((safe))
#define __force           __attribute__((force))
#define __nocast          __attribute__((nocast))
#define __iomem           __attribute__((noderef, address_space(2)))
#define __must_hold(x)    __attribute__((context(x, 1, 1)))
#define __acquires(x)     __attribute__((context(x, 0, 1)))
#define __releases(x)     __attribute__((context(x, 1, 0)))
#define __acquire(x)       __context__(x, 1)
#define __release(x)      __context__(x, -1)
#define __cond_lock(x, c) ((c) ? ({ __acquire(x); 1; }) : 0)
#define __percpu __attribute__((noderef, address_space(3)))
#define __used __attribute__((__used__))
#define __must_check __attribute__((warn_unused_result))

#define barrier() __asm__ __volatile__("": : :"memory")

#define noinline __attribute_((noinline))
#define __always_inline    inline __attribute__((always_inline))

#define __packed __attribute__((packed))

#define __printf(str, args)  __attribute__((format(printf, str, args)))


#endif

