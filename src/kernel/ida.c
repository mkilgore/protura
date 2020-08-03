/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/bits.h>
#include <protura/mm/ptable.h>
#include <protura/ida.h>

int ida_getid(struct ida *ida)
{
    using_spinlock(&ida->lock) {
        int id = bit_find_first_zero(ida->ids, ALIGN_2(ida->total_ids, 4));

        if (id == -1 || id >= ida->total_ids)
            return -1;

        bit_set(ida->ids, id);

        return id;
    }
}

void ida_putid(struct ida *ida, int id)
{
    using_spinlock(&ida->lock)
        bit_clear(ida->ids, id);
}
