/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TIME_H
#define INCLUDE_PROTURA_TIME_H

#include <protura/types.h>

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

#endif
