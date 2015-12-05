/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_USER_PTR_H
#define INCLUDE_MM_USER_PTR_H

#include <protura/types.h>
#include <protura/bits.h>
#include <arch/cpu.h>

/* Used to mark and check pointers from user-space. user_check_region() should
 * be used to verify that a pointer points into a valid piece of user-space
 * memory, and the __user marker should be used to identify user-space
 * pointers. */

/* Note that the __user macro is just a marker, it doesn't actually do anything
 * to the pointers type. */
#define __user

/* user_check_region() should be called to verity a valid points to a valid
 * piece of user-memory before attempting a read or write. user_write and
 * user_read are *not* checked for correctness and could page-fault -
 * user_check_region thus returns an error of a user_write or user_read could
 * possibly page-fault.
 *
 * Note that user_check_region is just a wrapper for the two more specalized
 * regions, user_check_region_writable() and user_check_region_readable(). */
int user_check_region(const void *ptr, size_t size, flags_t vm_flags);

/* Like user_check_region, but for NUL terminated strings - Allows the memory
 * region to be less then 'max_size' in length as long as it has a NUL
 * terminator within the valid memory region */
int user_check_strn(const void *ptr, size_t max_size, flags_t vm_flags);

#endif
