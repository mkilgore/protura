/*
 * Copyright (C) 2013 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/string.h>
#include <protura/debug.h>
#include <protura/snprintf.h>

void dump_mem(const void *buf, size_t len, uint32_t base_addr)
{
    int i = 0;

    for (i = 0; i < len; i += 4)
        kprintf("0x%x: 0x%x\n", base_addr + i, *(uint32_t *)((char *)buf + i));

}

