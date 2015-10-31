/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_ELF_H
#define INCLUDE_FS_ELF_H

#include <protura/types.h>
#include <protura/compiler.h>

struct elf_header {
    uint32_t magic;
    uint8_t  ident[12];
    uint16_t type;
    uint16_t machine;
    uint32_t elf_version;
    uint32_t entry_vaddr;
    uint32_t prog_head_pos;
    uint32_t sect_head_pos;
    uint32_t flags;
    uint16_t head_size;

    uint16_t prog_head_size;
    uint16_t prog_head_count;
    uint16_t sect_head_size;
    uint16_t sect_head_count;

    uint16_t str_index;
} __packed;

struct elf_prog_section {
    uint32_t type;
    uint32_t f_off;
    uint32_t vaddr;
    uint32_t undef;
    uint32_t f_size;
    uint32_t mem_size;
    uint32_t flags;
    uint32_t alginment;
} __packed;

/* The magic number for an ELF executable. "\x7FELF"
 *
 * Note that since we use this value as a 32-bit integer, we have to encode it
 * in little-endian. */
#define ELF_MAGIC 0x464C457FU

/* We only include flags for the important fields.
 *
 * This flag defines a 'loadable' section, the only type of section we support
 * currently. */
#define ELF_PROG_TYPE_LOAD 1

/* Flags for each section */
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4

void elf_register(void);
void elf_unregister(void);

#endif
