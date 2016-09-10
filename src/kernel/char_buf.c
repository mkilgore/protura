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

void char_buf_init(struct char_buf *buf, void *nbuffer, size_t buf_size)
{
    memset(buf, 0, sizeof(*buf));

    buf->buffer = nbuffer;
    buf->len = buf_size;

    buf->start_pos = 0;
    //buf->end_pos = 0;
    buf->buf_len = 0;
}

void char_buf_write_char(struct char_buf *buf, char data)
{
    int end_pos = (buf->start_pos + buf->buf_len) % buf->len;
    buf->buffer[end_pos] = data;
    buf->buf_len++;
    //if (buf->end_pos == buf->len)
    //    buf->end_pos = 0;
}

char char_buf_read_char(struct char_buf *buf)
{
    char data;

    if (buf->buf_len == 0)
        return -1;

    data = buf->buffer[buf->start_pos];

    buf->start_pos++;
    buf->buf_len--;
    if (buf->start_pos == buf->len)
        buf->start_pos = 0;

    return data;
}

void char_buf_write(struct char_buf *buf, const void *data, size_t data_len)
{
    int end_pos = (buf->start_pos + buf->buf_len) % buf->len;

    if (buf->len - end_pos >= data_len) {
        /* All the data can fit after end_pos, no wrapping nessisary */
        memcpy(buf->buffer + end_pos, data, data_len);
        buf->buf_len += data_len;
        //if (buf->end_pos == buf->len)
        //    buf->end_pos = 0;
    } else {
        /* Only part of the data fits after end_pos, we have to wrap for the
         * rest */
        if (buf->len - end_pos > 1) {
            memcpy(buf->buffer + end_pos, data, buf->len - end_pos - 1);

            buf->buf_len += buf->len - end_pos;
            data_len -= buf->len - end_pos;
            data += buf->len - end_pos;
        }

        memcpy(buf->buffer, data, data_len);
        //buf->end_pos = data_len;
        buf->buf_len += data_len;
    }
}

size_t char_buf_read(struct char_buf *buf, void *data, size_t data_len)
{
    size_t orig_size;

    if (!buf->buf_len)
        return 0;

    if (buf->buf_len < data_len)
        data_len = buf->buf_len;

    orig_size = data_len;

    kp(KP_TRACE, "char_buf_read data_len: %d\n", data_len);

    if (buf->len - buf->start_pos >= data_len) {
        kp(KP_TRACE, "First path\n");
        memcpy(data, buf->buffer + buf->start_pos, data_len);
        buf->start_pos += data_len;
        buf->buf_len -= data_len;

        if (buf->start_pos == buf->len)
            buf->start_pos = 0;

    } else {
        kp(KP_TRACE, "Second path\n");
        if (buf->len - buf->start_pos > 1) {
            memcpy(data, buf->buffer + buf->start_pos, buf->len - buf->start_pos - 1);

            data += buf->len - buf->start_pos;
            data_len -= buf->len - buf->start_pos;
        }

        memcpy(data, buf->buffer, data_len);
        buf->start_pos = data_len;
        buf->buf_len -= buf->len - buf->start_pos;
    }

    kp(KP_TRACE, "orig_size: %d, data_len: %d, result: %d\n", orig_size, data_len, orig_size - data_len);

    return data_len;
}

