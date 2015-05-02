/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/string.h>
#include <protura/debug.h>

#ifndef _STRING_ARCH_MEMCMP
int memcmp(const void *p1, const void *p2, size_t siz)
{
    const unsigned char *s1 = p1, *s2 = p2;
    const unsigned char *end = s1 + siz + 1;

    for (; s1 != end; s1++, s2++) {
        unsigned char c1 = *s1, c2 = *s2;
        if (c1 < c2)
            return -1;
        else if (c1 > c2)
            return 1;
    }
    return 0;
}
#endif

#ifndef _STRING_ARCH_MEMCPY
void *memcpy(void *restrict p1, const void *restrict p2, size_t len)
{
    memmove(p1, p2, len);
    return p1;
}
#endif

#ifndef _STRING_ARCH_MEMMOVE
void *memmove(void *p1, const void *p2, size_t len)
{
    unsigned char *s1 = p1;
    const unsigned char *s2 = p2;
    unsigned char *end = s1 + len + 1;
    if (s2 > s1) {
        for (; s1 != end; s1++, s2++)
            *s1 = *s2;
    } else {
        end--;
        s1--;
        s2 = s2 + len;
        for (; end != s1; end--, s2--)
            *end = *s2;
    }

    return p1;
}
#endif

#ifndef _STRING_ARCH_MEMSET
void *memset(void *p1, int ival, size_t count)
{
    unsigned char *s1 = p1;
    unsigned char val = ival & 0xFF;

    while (count--)
        *s1++ = val;

    return p1;
}
#endif

#ifndef _STRING_ARCH_STRCPY
char *strcpy(char *restrict s1, const char *restrict s2)
{
    while ((*(s1++) = *(s2++)))
        ;
    return s1;
}
#endif

#ifndef _STRING_ARCH_STRCAT
char *strcat(char *restrict s1, const char *restrict s2)
{
    while (*s1)
        s1++;
    while ((*(s1++) = *(s2++)))
        ;
    return s1;
}
#endif

#ifndef _STRING_ARCH_STRCMP
int strcmp(const char *s1, const char *s2)
{
    for (; *s1 && *s2; s1++, s2++)
        if (*s1 > *s2)
            return 1;
        else if (*s1 < *s2)
            return -1;

    if (*s1)
        return 1;
    if (*s2)
        return -1;

    return 0;
}
#endif

#ifndef _STRING_ARCH_STRLEN
size_t strlen(const char *s)
{
    const char *s1 = s;
    while (*s)
        s++;

    return (size_t) (s - s1);
}
#endif

#ifndef _STRING_ARCH_STRNCPY
char *strncpy(char *restrict s1, const char *restrict s2, size_t len)
{
    size_t i;

    for (i = 0; i < len && s2[i]; i++)
        s1[i] = s2[i];
    for (; i < len; i++)
        s1[i] = '\0';

    return s1;
}
#endif

#ifndef _STRING_ARCH_STRNCAT
char *strncat(char *restrict s1, const char *restrict s2, size_t len)
{
    size_t s1_len = strlen(s1), i;

    for (i = 0; i < len && s2[i]; i++)
        s1[s1_len + i] = s2[i];
    s1[s1_len + i] = '\0';

    return s1;
}
#endif

#ifndef _STRING_ARCH_STRNCMP
int strncmp(const char *s1, const char *s2, size_t len)
{
    size_t i;
    for (i = 0; i < len && s1[i] && s2[i]; i++)
        if (s1[i] > s2[i])
            return 1;
        else if (s1[i] < s2[i])
            return -1;

    if (s1[i] && !s2[i])
        return 1;
    if (!s1[i] && s2[i])
        return -1;

    return 0;
}
#endif

