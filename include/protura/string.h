/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_STRING_H
#define INCLUDE_PROTURA_STRING_H

#include <protura/types.h>
#include <arch/string.h>

#ifndef _STRING_ARCH_MEMCMP
int    memcmp(const void *, const void *, size_t);
#endif
#ifndef _STRING_ARCH_MEMCPY
void  *memcpy(void *restrict, const void *restrict, size_t);
#endif
#ifndef _STRING_ARCH_MEMMOVE
void  *memmove(void *, const void *, size_t);
#endif
#ifndef _STRING_ARCH_MEMSET
void  *memset(void *, int, size_t);
#endif

#ifndef _STRING_ARCH_STRCPY
char  *strcpy(char *restrict, const char *restrict);
#endif
#ifndef _STRING_ARCH_STRCAT
char  *strcat(char *restrict, const char *restrict);
#endif
#ifndef _STRING_ARCH_STRCMP
int    strcmp(const char *, const char *);
#endif
#ifndef _STRING_ARCH_STRLEN
size_t strlen(const char *);
#endif
#ifndef _STRING_ARCH_STRNLEN
size_t strnlen(const char *, size_t len);
#endif


#ifndef _STRING_ARCH_STRNCPY
char  *strncpy(char *restrict, const char *restrict, size_t len);
#endif
#ifndef _STRING_ARCH_STRNCAT
char  *strncat(char *restrict, const char *restrict, size_t len);
#endif
#ifndef _STRING_ARCH_STRNCMP
int    strncmp(const char *, const char *, size_t len);
#endif

#endif

