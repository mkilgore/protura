/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/string.h>

#ifdef _STRING_ARCH_STRCPY
char *strcpy(char *dest, const char *src)
{
    int d0, d1, d2;
    asm volatile(
            "1:\n"
            "lodsb;"
            "testb %%al, %%al;"
            "jne 1b"
            : "=&S" (d0), "=&D" (d1), "=&a" (d2)
            : "0" (src), "1" (dest)
            : "memory");
    return dest;
}
#endif

#ifdef _STRING_ARCH_STRCAT
char *strcat(char *dest, const char *src)
{
    int d0, d1, d2, d3;
    asm volatile(
            "repne;"
            "scasb;"
            "decl %1;"
            "\n1:\n"
            "lodsb;"
            "stosb;"
            "testb %%al, %%al;"
            "jne 1b"
            : "=&S" (d0), "=&D" (d1), "=&a" (d2), "=&c" (d3)
            : "0" (src), "1" (dest), "2" (0), "3" (0xFFFFFFFFu)
            : "memory");
    return dest;
}
#endif

#ifdef _STRING_ARCH_STRCMP
int strcmp(const char *cs, const char *ct)
{
    int d0, d1;
    int res;
    asm volatile(
            "1:\n"
            "lodsb;"
            "scasb;"
            "jne 2f;"
            "testb %%al, %%al;"
            "jne 1b;"
            "xorl %%eax, %%eax;"
            "jmp 3f;"
            "\n2:\n"
            "sbbl %%eax, %%eax;"
            "\n3:"
            : "=a" (res), "=&S" (d0), "=&D" (d1)
            : "1" (cs), "2" (ct)
            : "memory");
    return res;
}
#endif

#ifdef _STRING_ARCH_STRLEN
size_t strlen(const char *s)
{
    int d0;
    size_t res;
    asm volatile(
            "repne;"
            "scasb"
            : "=c" (res), "=&D" (d0)
            : "1" (s), "a" (0), "0" (0xFFFFFFFFu)
            : "memory");
    return ~res - 1;
}
#endif

