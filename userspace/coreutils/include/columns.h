#ifndef _COLUMNS_H
#define _COLUMNS_H

#include "list.h"

enum util_column_align {
    UTIL_ALIGN_LEFT,
    UTIL_ALIGN_RIGHT,
};

struct util_column {
    list_node_t entry;
    char *buf;
    enum util_column_align alignment;
};

struct util_line {
    list_node_t entry;
    list_head_t column_list;
    size_t column_count;
};

struct util_display {
    list_head_t line_list;
};

#define UTIL_COLUMN_INIT(c) \
    { \
        .entry = LIST_NODE_INIT((c).entry), \
    }

static inline void util_column_init(struct util_column *col)
{
    *col = (struct util_column)UTIL_COLUMN_INIT(*col);
}

#define UTIL_LINE_INIT(l) \
    { \
        .entry = LIST_NODE_INIT((l).entry), \
        .column_list = LIST_HEAD_INIT((l).column_list), \
    }

static inline void util_line_init(struct util_line *line)
{
    *line = (struct util_line)UTIL_LINE_INIT(*line);
}

#define UTIL_DISPLAY_INIT(d) \
    { \
        .line_list = LIST_HEAD_INIT((d).line_list), \
    }

static inline void util_display_init(struct util_display *display)
{
    *display = (struct util_display)UTIL_DISPLAY_INIT(*display);
}

void util_column_printfv(struct util_column *column, const char *fmt, va_list lst);
void util_column_printf(struct util_column *, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void util_column_strdup(struct util_column *, const char *);

/* Convience methods that automatically create columns */
void util_line_printf(struct util_line *, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void util_line_printf_ar(struct util_line *, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void util_line_strdup(struct util_line *, const char *);
void util_line_strdup_ar(struct util_line *, const char *);

struct util_column *util_line_next_column(struct util_line *);
struct util_line *util_display_next_line(struct util_display *);

void util_display_render(struct util_display *);

#endif
