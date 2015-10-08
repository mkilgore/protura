/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_LOG2_H
#define INCLUDE_ARCH_LOG2_H

#include <protura/types.h>

static inline uint32_t log2(uint32_t value)
{
  uint32_t ret;

  asm volatile ("bsr %1, %0": "=r"(ret): "r" (value));

  return ret;
}

#endif
