/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/crc.h>

uint16_t crc16(void *data, size_t len, uint16_t poly)
{
    uint8_t *d, *end;
    uint16_t crc = 0;
    int i;

    for (d = data, end = d + len; d != end; d++) {
        crc ^= *d;
        for (i = 0; i < 8; i++) {
            int flag = crc & 0x0001;

            crc >>= 1;
            if (flag)
                crc ^= poly;
        }
    }

    return crc;
}

