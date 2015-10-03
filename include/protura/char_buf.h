#ifndef INCLUDE_PROTURA_CHAR_BUF_H
#define INCLUDE_PROTURA_CHAR_BUF_H

#include <protura/types.h>

struct char_buf {
    char *buffer;
    ksize_t len;

    int start_pos;
    int end_pos;
};

void char_buf_init(struct char_buf *, void *buffer, ksize_t buf_size);

void char_buf_write_char(struct char_buf *buf, char data);
char char_buf_read_char(struct char_buf *buf);
void char_buf_write(struct char_buf *, void *data, ksize_t data_len);
void char_buf_read(struct char_buf *, void *data, ksize_t data_len);

#define char_buf_has_data(buf) ((buf)->start_pos != (buf)->end_pos)

#endif
