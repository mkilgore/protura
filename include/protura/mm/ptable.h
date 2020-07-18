/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_PTABLE_H
#define INCLUDE_MM_PTABLE_H

#include <arch/ptable.h>

void page_table_map_entry(pgd_t *table, va_t virtual, pa_t physical, flags_t vm_flags, int pcm);

/* Maps a linear physical range */
void page_table_map_range(pgd_t *table, va_t virtual, pa_t physical, int pages, flags_t vm_flags, int pcm);

void page_table_unmap_entry(pgd_t *table, va_t virtual);
pte_t *page_table_get_entry(pgd_t *table, va_t virtual);

/* This does a pfree() of all the mapped pages */
void page_table_free_range(pgd_t *table, va_t virtual, int pages);

/* Clears out a range of mappings without touching the underlying pages */
void page_table_zap_range(pgd_t *table, va_t virtual, int pages);

/* Creates copies of backing pages in a defined range */
void page_table_copy_range(pgd_t *new, pgd_t *old, va_t virtual, int pages);

/* Creates the exact same page mappings in the new pgd_t as the old pgd_t. The
 * underlying pages are not touched. */
void page_table_clone_range(pgd_t *new, pgd_t *old, va_t virtual, int pages);

/* Verifies that a pointer is mapped to backing memory in the provided page
 * directory. */
int pgd_ptr_is_valid(pgd_t *, va_t);

#endif
