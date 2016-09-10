#ifndef INCLUDE_PROTURA_CHAR_BUF_H
#define INCLUDE_PROTURA_CHAR_BUF_H

#include <protura/types.h>

struct char_buf {
    char *buffer;
    size_t len;

    int start_pos;
    int buf_len;
};

void char_buf_init(struct char_buf *, void *buffer, size_t buf_size);

void char_buf_write_char(struct char_buf *buf, char data);
char char_buf_read_char(struct char_buf *buf);
void char_buf_write(struct char_buf *, const void *data, size_t data_len);
size_t char_buf_read(struct char_buf *, void *data, size_t data_len);

#define char_buf_has_data(buf) ((buf)->buf_len)

#endif
