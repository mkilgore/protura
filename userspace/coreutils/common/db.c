
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "db.h"

int db_entry_comma_sep(struct db_entry *ent)
{
    return 0;
}

void db_row_add_string_entry(struct db_row *row, const char *str)
{
    struct db_entry *e;

    e = malloc(sizeof(*e));
    db_entry_init(e);
    e->contents = strdupx(str);
    list_add_tail(&row->entry_list, &e->row_entry);
}

void db_row_add_int_entry(struct db_row *row, int val)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%d", val);
    db_row_add_string_entry(row, buf);
}

static void append_empty_entries(struct db_row *row, int extra)
{
    int i;
    for (i = 0; i < extra; i++) {
        struct db_entry *ent = malloc(sizeof(*ent));
        db_entry_init(ent);

        list_add_tail(&row->entry_list, &ent->row_entry);
    }
}

int db_read(struct db *db, FILE *file)
{
    char *line = NULL;
    size_t buf_len = 0;
    ssize_t len = 0;
    int ret = 0;

    ret = fseek(file, 0, SEEK_SET);
    if (ret) {
        fprintf(stderr, "%s: Error seeking in file.\n", prog_name);
        return 1;
    }

    while ((len = getline(&line, &buf_len, file)) != EOF) {
        char *l = line;
        char *val;

        /* Remove the newline character */
        line[len - 1] = '\0';

        if (db->remove_whitespace)
            while (*l && (*l == ' ' || *l == '\t'))
                l++;

        if (!*l)
            continue;

        if (db->allow_comments && *l == '#')
            continue;

        struct db_row *row = malloc(sizeof(*row));
        db_row_init(row);

        list_add_tail(&db->rows, &row->db_entry);

        int count = 0;
        struct db_entry *ent;

        while ((val = strsep(&l, ":"))) {
            ent = malloc(sizeof(*ent));
            db_entry_init(ent);

            ent->contents = strdupx(val);
            list_add_tail(&row->entry_list, &ent->row_entry);
        }

        if (db->column_count > count)
            append_empty_entries(row, db->column_count - count);
    }

    return ret;
}

int db_write(struct db *db, FILE *file)
{
    struct db_row *row;
    struct db_entry *ent;

    int ret = fseek(file, 0, SEEK_SET);
    if (ret) {
        fprintf(stderr, "%s: Error seeking in file.\n", prog_name);
        return 1;
    }

    list_foreach_entry(&db->rows, row, db_entry) {
        int is_first = 1;

        list_foreach_entry(&row->entry_list, ent, row_entry) {
            if (!is_first)
                fputc(':', file);
            else
                is_first = 0;

            if (ent->contents)
                fprintf(file, "%s", ent->contents);
        }

        fputc('\n', file);
    }

    return 0;
}

void db_entry_clear(struct db_entry *ent)
{
    if (ent->contents)
        free(ent->contents);
}

void db_row_clear(struct db_row *row)
{
    struct db_entry *ent;
    list_foreach_take_entry(&row->entry_list, ent, row_entry) {
        db_entry_clear(ent);
        free(ent);
    }
}

void db_clear(struct db *db)
{
    struct db_row *row;

    list_foreach_take_entry(&db->rows, row, db_entry) {
        db_row_clear(row);
        free(row);
    }
}
