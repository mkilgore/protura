/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/strtol.h>

#define hexdigit(d) (((d) >= '0' && (d) <= '9') || ((d) >= 'a' && (d) <= 'z') || ((d) >= 'A' && (d) <= 'Z'))

/* Note: 'd' has to be a digit acording to 'hexdigit' */
#define digitval(d)              \
    (((d) >= '0' && (d) <= '9')? \
        ((d) - '0'):             \
    ((d) >= 'a' && (d) <= 'z')?  \
        ((d) - 'a' + 10):        \
        ((d) - 'A' + 10))


long strtol(const char *str, const char **endp, unsigned int base)
{
    long ret = 0;

    /* Decide on our base if we're given 0 */
    if (!base) {
        base = 10;
        if (str[0] == '0') {
            base = 8;
            str++;
            if (str[0] == 'x') {
                str++;
                base = 16;
            }
        }
    }

    for (; hexdigit(str[0]); str++) {
        long val = digitval(str[0]);
        if (val >= base)
            break;

        ret = ret * base + val;
    }

    if (endp)
        *endp = str;

    return ret;
}

