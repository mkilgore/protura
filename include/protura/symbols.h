/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_SYMBOLS_H
#define INCLUDE_PROTURA_SYMBOLS_H

#include <protura/types.h>

struct symbol {
    uintptr_t addr;
    size_t size;
    const char *name;
};

/* Returns the symbol that the specified address belongs too, or NULL if none */
const struct symbol *ksym_lookup(uintptr_t addr);

#endif
