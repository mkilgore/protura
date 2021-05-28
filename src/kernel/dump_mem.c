/*
 * Copyright (C) 2013 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/string.h>
#include <protura/debug.h>
#include <protura/snprintf.h>

void dump_mem(const void *buf, size_t len, uint32_t base_addr)
{
    char strbuf[200], strbuf2[200] = { 0 };
    char *cur_b, *start, *bufend, *to_print;
    const unsigned char *b = buf;
    int i = 0, j, skipping = 0;

    cur_b = strbuf;
    start = strbuf;

    for (; i < len; i += 16) {
        bufend = cur_b + sizeof(strbuf);
        cur_b += snprintf(cur_b, bufend - cur_b,  "%08x  ", (i) + base_addr);

        for (j = i; j < i + 16; j++) {
            if (j < len)
                cur_b += snprintf(cur_b, bufend - cur_b, "%02x ", (const unsigned int)b[j]);
            else
                cur_b += snprintf(cur_b, bufend - cur_b, "   ");

            if (j - i == 7)
                *(cur_b++) = ' ';
        }

        to_print = start;

        if (start == strbuf)
            start = strbuf2;
        else
            start = strbuf;

        cur_b = start;

        /* The 12 magic number is just so we don't compare the printed address,
         * which is in '0x00000000' format at the beginning of the string */
        if (strcmp(strbuf + 12, strbuf2 + 12) != 0) {
            if (skipping == 1)
                kp(KP_NORMAL, "%s", start);
            else if (skipping == 2)
                kp(KP_NORMAL, "...\n");
            skipping = 0;
            kp(KP_NORMAL, "%s\n", to_print);
        } else if (skipping >= 1) {
            skipping = 2;
        } else {
            skipping = 1;
        }
    }

    if (skipping) {
        kp(KP_NORMAL, "...\n");
        kp(KP_NORMAL, "%s\n", to_print);
    }
}

