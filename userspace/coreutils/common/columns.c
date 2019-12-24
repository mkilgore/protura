
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "columns.h"

void util_column_printfv(struct util_column *column, const char *fmt, va_list lst)
{
    if (column->buf)
        free(column->buf);

    column->buf = NULL;

    a_sprintfv(&column->buf, fmt, lst);
}

void util_column_printf(struct util_column *column, const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);

    util_column_printfv(column, fmt, lst);
}

void util_column_strdup(struct util_column *column, const char *str)
{
    if (column->buf)
        free(column->buf);

    column->buf = strdup(str);
}

void util_line_printf(struct util_line *line, const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);

    util_column_printfv(util_line_next_column(line), fmt, lst);
}

void util_line_strdup(struct util_line *line, const char *str)
{
    util_column_strdup(util_line_next_column(line), str);
}

void util_line_printf_ar(struct util_line *line, const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);

    struct util_column *column = util_line_next_column(line);

    column->alignment = UTIL_ALIGN_RIGHT;
    util_column_printfv(column, fmt, lst);
}

void util_line_strdup_ar(struct util_line *line, const char *str)
{
    struct util_column *column = util_line_next_column(line);

    column->alignment = UTIL_ALIGN_RIGHT;
    util_column_strdup(column, str);
}

struct util_column *util_line_next_column(struct util_line *line)
{
    struct util_column *col = malloc(sizeof(*col));
    util_column_init(col);

    list_add_tail(&line->column_list, &col->entry);

    line->column_count++;

    return col;
}

struct util_line *util_display_next_line(struct util_display *display)
{
    struct util_line *line = malloc(sizeof(*line));
    util_line_init(line);

    list_add_tail(&display->line_list, &line->entry);
    return line;
}

void util_display_render(struct util_display *display)
{
    struct util_line *line;
    struct util_column *column;
    size_t i;
    size_t max_columns = 0;

    list_foreach_entry(&display->line_list, line, entry)
        if (line->column_count > max_columns)
            max_columns = line->column_count;

    size_t column_lengths[max_columns];
    memset(column_lengths, 0, sizeof(column_lengths));

    list_foreach_entry(&display->line_list, line, entry) {
        i = 0;
        list_foreach_entry(&line->column_list, column, entry) {
            size_t slen = strlen(column->buf);
            if (slen > column_lengths[i])
                column_lengths[i] = slen;

            i++;
        }
    }

    if (!column_lengths[0])
        return;

    list_foreach_entry(&display->line_list, line, entry) {
        i = 0;
        list_foreach_entry(&line->column_list, column, entry) {
            if (column->alignment == UTIL_ALIGN_RIGHT)
                printf("%*s", (int)column_lengths[i], column->buf);
            else
                printf("%-*s", (int)column_lengths[i], column->buf);

            if (i < max_columns - 1)
                putchar(' ');

            i++;
        }

        putchar('\n');
    }
}
