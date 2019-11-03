/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_CMDLINE_H
#define INCLUDE_PROTURA_CMDLINE_H

void kernel_cmdline_init(void);
int kernel_cmdline_get_bool(const char *, int def);
const char *kernel_cmdline_get_string(const char *, const char *def);

#endif
