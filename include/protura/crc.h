/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_CRC_H
#define INCLUDE_PROTURA_CRC_H

#include <protura/types.h>

uint16_t crc16(void *data, size_t len, uint16_t poly);

#define CRC_ANSI_POLY ((uint16_t)0x8005)

#endif
