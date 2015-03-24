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
void irq_handler_end(void);

extern void (*irq_hands[256])(void);

#endif
