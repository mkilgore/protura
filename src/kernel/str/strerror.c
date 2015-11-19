/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/stddef.h>
#include <protura/errors.h>

#define ERR(val) \
    [val] = #val

const char *error_strings[] = {
    [0] = "SUCCESS",
#include "errors.x"
};

