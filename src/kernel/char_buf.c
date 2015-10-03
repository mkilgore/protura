/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/debug.h>
#include <protura/char_buf.h>

void char_buf_init(struct char_buf *buf, void *nbuffer, ksize_t buf_size)
{
    memset(buf, 0, sizeof(*buf));

    buf->buffer = nbuffer;
    buf->len = buf_size;

    buf->start_pos = 0;
    buf->end_pos = 0;
}

void char_buf_write_char(struct char_buf *buf, char data)
{
    buf->buffer[buf->end_pos] = data;
    buf->end_pos++;
    if (buf->end_pos == buf->len)
        buf->end_pos = 0;
}

char char_buf_read_char(struct char_buf *buf)
{
    char data;

    data = buf->buffer[buf->start_pos];

    buf->start_pos++;
    if (buf->start_pos == buf->len)
        buf->start_pos = 0;

    return data;
}

void char_buf_write(struct char_buf *buf, void *data, ksize_t data_len)
{
    if (buf->len - buf->end_pos >= data_len) {
        /* All the data can fit after end_pos, no wrapping nessisary */
        memcpy(buf->buffer + buf->end_pos, data, data_len);
        buf->end_pos += data_len;
        if (buf->end_pos == buf->len)
            buf->end_pos = 0;
    } else {
        /* Only part of the data fits after end_pos, we have to wrap for the
         * rest */
        if (buf->len - buf->end_pos > 0) {
            memcpy(buf->buffer + buf->end_pos, data, buf->len - buf->end_pos);

            data_len -= buf->len - buf->end_pos;
            data += buf->len - buf->end_pos;
        }

        memcpy(buf->buffer, data, data_len);
        buf->end_pos = data_len;
    }
}

void char_buf_read(struct char_buf *buf, void *data, ksize_t data_len)
{
    if (buf->len - buf->start_pos >= data_len) {
        memcpy(data, buf->buffer + buf->start_pos, data_len);
        buf->start_pos += data_len;
        if (buf->start_pos == buf->len)
            buf->start_pos = 0;
        return ;
    } else {
        if (buf->len - buf->start_pos > 0) {
            memcpy(data, buf->buffer + buf->start_pos, buf->len - buf->start_pos);

            data += buf->len - buf->start_pos;
            data_len -= buf->len - buf->start_pos;
        }

        memcpy(data, buf->buffer, data_len);
        buf->start_pos = data_len;
    }
}

