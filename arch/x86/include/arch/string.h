/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_STRING_H
#define INCLUDE_ARCH_STRING_H

#include <protura/compiler.h>
#include <protura/types.h>

#define _STRING_ARCH_MEMCMP
#define memcmp(t, f, n) __builtin_memcmp(t, f, n)

#define _STRING_ARCH_MEMCPY

static __always_inline void *__memcpy(void *to, const void *from, size_t n)
{
	int d0, d1, d2;
	asm volatile("rep ; movsl\n\t"
		     "movl %4,%%ecx\n\t"
		     "andl $3,%%ecx\n\t"
		     "jz 1f\n\t"
		     "rep ; movsb\n\t"
		     "1:"
		     : "=&c" (d0), "=&D" (d1), "=&S" (d2)
		     : "0" (n / 4), "g" (n), "1" ((long)to), "2" ((long)from)
		     : "memory");
	return to;
}

/*
 * This looks ugly, but the compiler can optimize it totally,
 * as the count is constant.
 */
static __always_inline void *__constant_memcpy(void *to, const void *from,
					       size_t n)
{
	long esi, edi;
	if (!n)
		return to;

	switch (n) {
	case 1:
		*(char *)to = *(char *)from;
		return to;
	case 2:
		*(short *)to = *(short *)from;
		return to;
	case 4:
		*(int *)to = *(int *)from;
		return to;
	case 3:
		*(short *)to = *(short *)from;
		*((char *)to + 2) = *((char *)from + 2);
		return to;
	case 5:
		*(int *)to = *(int *)from;
		*((char *)to + 4) = *((char *)from + 4);
		return to;
	case 6:
		*(int *)to = *(int *)from;
		*((short *)to + 2) = *((short *)from + 2);
		return to;
	case 8:
		*(int *)to = *(int *)from;
		*((int *)to + 1) = *((int *)from + 1);
		return to;
	}

	esi = (long)from;
	edi = (long)to;
	if (n >= 5 * 4) {
		/* large block: use rep prefix */
		int ecx;
		asm volatile("rep ; movsl"
			     : "=&c" (ecx), "=&D" (edi), "=&S" (esi)
			     : "0" (n / 4), "1" (edi), "2" (esi)
			     : "memory"
		);
	} else {
		/* small block: don't clobber ecx + smaller code */
		if (n >= 4 * 4)
			asm volatile("movsl"
				     : "=&D"(edi), "=&S"(esi)
				     : "0"(edi), "1"(esi)
				     : "memory");
		if (n >= 3 * 4)
			asm volatile("movsl"
				     : "=&D"(edi), "=&S"(esi)
				     : "0"(edi), "1"(esi)
				     : "memory");
		if (n >= 2 * 4)
			asm volatile("movsl"
				     : "=&D"(edi), "=&S"(esi)
				     : "0"(edi), "1"(esi)
				     : "memory");
		if (n >= 1 * 4)
			asm volatile("movsl"
				     : "=&D"(edi), "=&S"(esi)
				     : "0"(edi), "1"(esi)
				     : "memory");
	}
	switch (n % 4) {
		/* tail */
	case 0:
		return to;
	case 1:
		asm volatile("movsb"
			     : "=&D"(edi), "=&S"(esi)
			     : "0"(edi), "1"(esi)
			     : "memory");
		return to;
	case 2:
		asm volatile("movsw"
			     : "=&D"(edi), "=&S"(esi)
			     : "0"(edi), "1"(esi)
			     : "memory");
		return to;
	default:
		asm volatile("movsw\n\tmovsb"
			     : "=&D"(edi), "=&S"(esi)
			     : "0"(edi), "1"(esi)
			     : "memory");
		return to;
	}
}

#define memcpy(t, f, n) \
    (__builtin_constant_p((n)) \
     ? __constant_memcpy((t), (f), (n)) \
     : __memcpy((t), (f), (n)))

#define _STRING_ARCH_MEMMOVE
#define memmove(t, f, n) __builtin_memmove(t, f, n)

#define _STRING_ARCH_MEMSET
#define memset(s, c, count) __builtin_memset(s, c, count)

#define _STRING_ARCH_STRCPY
char *strcpy(char *dest, const char *src);

#define _STRING_ARCH_STRCAT
char *strcat(char *dest, const char *src);

#define _STRING_ARCH_STRCMP
int strcmp(const char *dest, const char *src);

#define _STRING_ARCH_STRLEN
size_t strlen(const char *str);

#endif
