/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/basic_printf.h>
#include <protura/string.h>
#include <protura/errors.h>
#include <protura/mm/palloc.h>
#include <protura/mm/user_check.h>
#include <protura/kbuf.h>

int kbuf_add_page(struct kbuf *kbuf)
{
    struct page *page = palloc(0, PAL_KERNEL);
    if (!page)
        return -ENOMEM;

    kbuf->page_count++;
    list_add_tail(&kbuf->pages, &page->page_list_node);
    return 0;
}

void kbuf_clear(struct kbuf *kbuf)
{
    struct page *page;
    list_foreach_take_entry(&kbuf->pages, page, page_list_node)
        pfree(page, 0);

    kbuf->page_count = 0;
    kbuf->cur_pos.offset = 0;
}

static size_t kbuf_get_total_length(struct kbuf *kbuf)
{
    return kbuf->page_count * PG_SIZE;
}

size_t kbuf_get_free_length(struct kbuf *kbuf)
{
    return kbuf_get_total_length(kbuf) - kbuf->cur_pos.offset;
}

size_t kbuf_get_length(struct kbuf *kbuf)
{
    return kbuf->cur_pos.offset;
}

struct kbuf_pos kbuf_get_pos(struct kbuf *kbuf)
{
    return kbuf->cur_pos;
}

void kbuf_reset_pos(struct kbuf *kbuf, struct kbuf_pos pos)
{
    kbuf->cur_pos = pos;
}

static struct page *kbuf_get_cur_page(struct kbuf *kbuf, size_t offset)
{
    int page_off = offset / PG_SIZE;
    int count = 0;
    struct page *page;

    list_foreach_entry(&kbuf->pages, page, page_list_node) {
        if (count == page_off)
            return page;

        count++;
    }

    return NULL;
}

int kbuf_read(struct kbuf *kbuf, size_t loc, struct user_buffer buf, size_t len)
{
    struct page *cur_page = kbuf_get_cur_page(kbuf, loc);
    if (!cur_page)
        return 0;

    size_t page_offset = loc % PG_SIZE;

    if (kbuf_get_length(kbuf) < loc + len)
        len = kbuf_get_length(kbuf) - loc;

    size_t have_read = 0;

    while (have_read < len) {
        /* Calculate how much to read, based on which we're reaching the end of
         * the read, or the end of a page boundary */
        size_t read_count = (len - have_read > PG_SIZE - page_offset)?
                                PG_SIZE - page_offset:
                                len - have_read;

        int ret = user_memcpy_from_kernel(user_buffer_index(buf, have_read), cur_page->virt + page_offset, read_count);
        if (ret)
            return ret;

        page_offset = 0;
        have_read += read_count;

        cur_page = list_next_entry(cur_page, page_list_node);

        /* Sanity check - if that was the last page, we should be done reading anyway */
        if (list_ptr_is_head(&kbuf->pages, &cur_page->page_list_node))
            break;
    }

    return have_read;
}

int kbuf_write(struct kbuf *kbuf, struct user_buffer buf, size_t len)
{
    struct page *cur_page = kbuf_get_cur_page(kbuf, kbuf->cur_pos.offset);
    if (!cur_page)
        return 0;

    if (kbuf_get_free_length(kbuf) < len)
        len = kbuf_get_free_length(kbuf);

    size_t page_offset = kbuf->cur_pos.offset % PG_SIZE;

    size_t have_written = 0;

    while (have_written < len) {
        size_t write_count = (len - have_written > PG_SIZE - page_offset)?
                                PG_SIZE - page_offset:
                                len - have_written;

        int ret = user_memcpy_to_kernel(cur_page->virt + page_offset, user_buffer_index(buf, have_written), write_count);
        if (ret)
            return ret;

        page_offset = 0;
        have_written += write_count;

        cur_page = list_next_entry(cur_page, page_list_node);

        /* Sanity check - if that was the last page, we should be done writing anyway */
        if (list_ptr_is_head(&kbuf->pages, &cur_page->page_list_node))
            break;
    }

    kbuf->cur_pos.offset += have_written;
    return have_written;
}

struct kbuf_backbone {
    struct printf_backbone backbone;
    struct kbuf *kbuf;
    int written;
    unsigned int hit_end :1;
};

static void kbuf_putchar(struct printf_backbone *b, char ch)
{
    struct kbuf_backbone *kb = container_of(b, struct kbuf_backbone, backbone);
    int ret = kbuf_write(kb->kbuf, make_kernel_buffer(&ch), 1);
    kb->written += ret;

    if (!ret)
        kb->hit_end = 1;
}

static void kbuf_putnstr(struct printf_backbone *b, const char *str, size_t len)
{
    struct kbuf_backbone *kb = container_of(b, struct kbuf_backbone, backbone);

    int ret = kbuf_write(kb->kbuf, make_kernel_buffer(str), len);
    kb->written += ret;

    if (ret < len)
        kb->hit_end = 1;
}

int kbuf_printfv(struct kbuf *kbuf, const char *fmt, va_list args)
{
    struct kbuf_backbone backbone = {
        .backbone = PRINTF_BACKBONE(kbuf_putchar, kbuf_putnstr),
        .kbuf = kbuf,
    };

    basic_printfv(&backbone.backbone, fmt, args);

    if (backbone.hit_end)
        return -ENOSPC;
    else
        return backbone.written;
}

int kbuf_printf(struct kbuf *kbuf, const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);

    int ret = kbuf_printfv(kbuf, fmt, lst);

    va_end(lst);
    return ret;
}

#ifdef CONFIG_KERNEL_TESTS
# include "kbuf_test.c"
#endif
