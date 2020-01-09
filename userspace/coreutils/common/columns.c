
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

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

static const char *util_column_color_fg_str[] = {
    [COL_FG_NORMAL]       = TERM_COLOR_NORMAL,
    [COL_FG_RED]          = TERM_COLOR_RED,
    [COL_FG_GREEN]        = TERM_COLOR_GREEN,
    [COL_FG_YELLOW]       = TERM_COLOR_YELLOW,
    [COL_FG_BLUE]         = TERM_COLOR_BLUE,
    [COL_FG_MAGENTA]      = TERM_COLOR_MAGENTA,
    [COL_FG_CYAN]         = TERM_COLOR_CYAN,
    [COL_FG_BOLD_RED]     = TERM_COLOR_BOLD_RED,
    [COL_FG_BOLD_GREEN]   = TERM_COLOR_BOLD_GREEN,
    [COL_FG_BOLD_YELLOW]  = TERM_COLOR_BOLD_YELLOW,
    [COL_FG_BOLD_BLUE]    = TERM_COLOR_BOLD_BLUE,
    [COL_FG_BOLD_MAGENTA] = TERM_COLOR_BOLD_MAGENTA,
    [COL_FG_BOLD_CYAN]    = TERM_COLOR_BOLD_CYAN,
};

static const char *util_column_color_bg_str[] = {
    [COL_BG_NORMAL]  = TERM_COLOR_NORMAL,
    [COL_BG_RED]     = TERM_COLOR_BG_RED,
    [COL_BG_GREEN]   = TERM_COLOR_BG_GREEN,
    [COL_BG_YELLOW]  = TERM_COLOR_BG_YELLOW,
    [COL_BG_BLUE]    = TERM_COLOR_BG_BLUE,
    [COL_BG_MAGENTA] = TERM_COLOR_BG_MAGENTA,
    [COL_BG_CYAN]    = TERM_COLOR_BG_CYAN,
};

void util_display_render(struct util_display *display)
{
    struct util_line *line;
    struct util_column *column;
    size_t i;
    size_t max_columns = 0;
    int show_colors = isatty(STDOUT_FILENO);

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
            if (show_colors) {
                fputs(util_column_color_fg_str[column->fg_color], stdout);
                fputs(util_column_color_bg_str[column->bg_color], stdout);
            }

            if (column->alignment == UTIL_ALIGN_RIGHT)
                printf("%*s", (int)column_lengths[i], column->buf);
            else
                printf("%-*s", (int)column_lengths[i], column->buf);

            if (show_colors)
                fputs(TERM_COLOR_RESET, stdout);

            if (i < max_columns - 1)
                putchar(' ');

            i++;
        }

        putchar('\n');
    }
}
