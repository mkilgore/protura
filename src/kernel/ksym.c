/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#include <protura/types.h>
#include <protura/symbols.h>

/* The Weak symbol attribute allows linking to complete during the first
 * linking attempt when kernel_symbols does not exist, but also allows the real
 * definition to take over when we link the actual symbol table */
extern const struct symbol kernel_symbols[] __weak;

const struct symbol *ksym_lookup(uintptr_t addr)
{
    const struct symbol *sym;

    for (sym = kernel_symbols; sym->name; sym++)
        if (sym->addr <= addr && (sym->addr + sym->size) >= addr)
            return sym;

    return NULL;
}
