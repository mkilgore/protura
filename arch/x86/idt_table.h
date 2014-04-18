/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_INTERNAL_ARCH_IDT_TABLE_H
#define INCLUDE_INTERNAL_ARCH_IDT_TABLE_H

#include <protura/types.h>

void idt_flush(uint32_t);

/* Define isr's to link into idt table */
#define i(n) \
    extern void isr ## n (void)
i(0);  i(1);  i(2);  i(3);  i(4);  i(5);
i(6);  i(7);  i(8);  i(9);  i(10); i(11);
i(12); i(13); i(14); i(15); i(16); i(17);
i(18); i(19); i(20); i(21); i(22); i(23);
i(24); i(25); i(26); i(27); i(28); i(29);
i(30); i(31);
#undef i

#endif
