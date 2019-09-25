#ifndef INCLUDE_PROTURA_KBUF_H
#define INCLUDE_PROTURA_KBUF_H

#include <protura/types.h>
#include <protura/stddef.h>
#include <protura/list.h>

/*
 * kbuf implements a simple buffer interface that allows writing to the end,
 * and reading from any location. The buffer size can be expanded by requesting
 * a new page be added. You can also reset the write location (In situations of
 * things like a partial write that you want to roll back).
 */

struct kbuf_pos {
    size_t offset;
};

struct kbuf {
    list_head_t pages;
    size_t page_count;
    struct kbuf_pos cur_pos;
};

#define KBUF_INIT(kb) \
    { \
        .pages = LIST_HEAD_INIT((kb).pages), \
    }

static inline void kbuf_init(struct kbuf *kbuf)
{
    *kbuf = (struct kbuf)KBUF_INIT(*kbuf);
}

int kbuf_add_page(struct kbuf *);

void kbuf_clear(struct kbuf *);

size_t kbuf_get_free_length(struct kbuf *);
size_t kbuf_get_length(struct kbuf *);

struct kbuf_pos kbuf_get_pos(struct kbuf *);
void kbuf_reset_pos(struct kbuf *, struct kbuf_pos);

int kbuf_read(struct kbuf *, size_t loc, void *, size_t len);
int kbuf_write(struct kbuf *, const void *, size_t);

/*
 * NOTE: On an overflow, these return -ENOSPC
 */
int kbuf_printfv(struct kbuf *, const char *, va_list args);
int kbuf_printf(struct kbuf *, const char *, ...) __printf(2, 3);

#endif
