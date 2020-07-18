/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_PTABLE_H
#define INCLUDE_ARCH_PTABLE_H

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/list.h>
#include <protura/bits.h>
#include <arch/paging.h>

struct page_directory_entry {
    union {
        uint32_t entry;
        struct {
            uint32_t present :1;
            uint32_t writable :1;
            uint32_t user_page :1;
            uint32_t write_through :1;
            uint32_t cache_disabled :1;
            uint32_t accessed :1;
            uint32_t zero :1;
            uint32_t page_size :1;
            uint32_t ignored :1;
            uint32_t reserved :3;
            uint32_t addr :20;
        };
    };
};

struct page_table_entry {
    union {
        uint32_t entry;
        struct {
            uint32_t present :1;
            uint32_t writable :1;
            uint32_t user_page :1;
            uint32_t write_through :1;
            uint32_t cache_disabled :1;
            uint32_t accessed :1;
            uint32_t dirty :1;
            uint32_t pat :1;
            uint32_t global :1;
            uint32_t reserved :3;
            uint32_t addr :20;
        };
    };
};

struct page_directory {
    struct page_directory_entry entries[1024];
};

struct page_table {
    struct page_table_entry entries[1024];
};

typedef struct page_directory pgd_t;
typedef struct page_table pgt_t;

typedef struct page_directory_entry pde_t;
typedef struct page_table_entry pte_t;

#define PGD_INDEXES (ARRAY_SIZE(((struct page_directory *)NULL)->entries))
#define PGT_INDEXES (ARRAY_SIZE(((struct page_table *)NULL)->entries))

extern struct page_directory kernel_dir;

/* Allocates a new empty page directory */
pgd_t *page_table_new(void);

/* This just frees the page table */
void page_table_free(pgd_t *);

void page_table_change(pgd_t *new);

/* Sets the page cache mode setting on the supplied pte_t */
void pte_set_pcm(pte_t *pte, int pcm);

#define mk_pde(addr, flags) ((pde_t){ .entry = PAGING_FRAME((addr)) | flags })
#define mk_pte(addr, flags) ((pte_t){ .entry = PAGING_FRAME((addr)) | flags })

#define pgd_offset(v) (PAGING_DIR_INDEX((v)))
#define pgt_offset(v) (PAGING_TABLE_INDEX((v)))

#define pgd_get_pde(pd, v) ((pd)->entries + pgd_offset((v)))
#define pgt_get_pte(pd, v) ((pd)->entries + pgt_offset((v)))

#define pde_to_pgt(pde) ((pgt_t *)P2V(PAGING_FRAME((pde)->entry)))

#define pgd_get_pde_offset(pd, o) ((pd)->entries + (o))
#define pgt_get_pte_offset(pd, o) ((pd)->entries + (o))

#define pde_exists(pde) ((pde)->present)
#define pde_writable(pde) ((pde)->writable)
#define pde_is_user(pde) ((pde)->user_page)
#define pde_is_huge(pde) ((pde)->page_size)

#define pde_set_writable(pde) ((pde)->writable = 1)
#define pde_unset_writable(pde) ((pde)->writable = 0)

#define pde_set_user(pde) ((pde)->user_page = 1)
#define pde_unset_user(pde) ((pde)->user_page = 0)

#define pde_get_pa(pd) PAGING_FRAME((pd)->entry)

#define pde_set_pa(pde, pa) \
    do { \
        (pde)->addr = __PA_TO_PN(pa); \
        (pde)->present = 1; \
    } while (0)

#define pde_clear_pa(pde) \
    do { \
        (pde)->addr = 0; \
        (pde)->present = 0; \
    } while (0)

#define pte_exists(pte) ((pte)->present)
#define pte_writable(pte) ((pte)->writable)
#define pte_is_user(pte) ((pte)->user_page)

#define pte_set_writable(pte) ((pte)->writable = 1)
#define pte_unset_writable(pte) ((pte)->writable = 0)

#define pte_set_user(pte) ((pte)->user_page = 1)
#define pte_unset_user(pte) ((pte)->user_page = 0)

#define pte_get_pa(pd) PAGING_FRAME((pd)->entry)

#define pte_set_pa(pte, pa) \
    do { \
        (pte)->addr = __PA_TO_PN(pa); \
        (pte)->present = 1; \
    } while (0)

#define pte_clear_pa(pte) \
    do { \
        (pte)->addr = 0; \
        (pte)->present = 0; \
    } while (0)

#define pgd_foreach_pde(pgd, pde) __pgd_foreach_pde_unique((pgd), (pde), __COUNTER__)

#define __pgd_foreach_pde_unique(pgd, pde, unique) \
    for (size_t TP(_pde_state, unique) = 0; \
         TP(_pde_state, unique) < ARRAY_SIZE((*pgd).entries); \
         TP(_pde_state, unique)++) \
        if (1) { \
            pde = (pgd)->entries + TP(_pde_state, unique); \
            goto TP(_pde_body, unique); \
        } else \
            TP(_pde_body, unique):

#define pgt_foreach_pte(pgt, pte) __pgt_foreach_pte_unique((pgt), (pte), __COUNTER__)

#define __pgt_foreach_pte_unique(pgt, pte, unique) \
    for (size_t TP(_pte_state, unique) = 0; \
         TP(_pte_state, unique) < ARRAY_SIZE((*pgt).entries); \
         TP(_pte_state, unique)++) \
        if (1) { \
            pte = (pgt)->entries + TP(_pte_state, unique); \
            goto TP(_pte_body, unique); \
        } else \
            TP(_pte_body, unique):

#endif
