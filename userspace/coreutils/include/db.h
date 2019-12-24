#ifndef _DB_H
#define _DB_H

#include <stdio.h>
#include "list.h"

struct db_entry {
    list_node_t row_entry;
    char *contents;
};

struct db_row {
    list_head_t entry_list;
    list_node_t db_entry;
};

struct db {
    list_head_t rows;
    int column_count;
    unsigned int allow_comments :1; /* '#' comments */
    unsigned int remove_whitespace :1; /* Leading whitespace on first line */
};

#define DB_ENTRY_INIT(e) \
    { \
        .row_entry = LIST_NODE_INIT((e).row_entry), \
    }

#define DB_ROW_INIT(r) \
    { \
        .entry_list = LIST_HEAD_INIT((r).entry_list), \
        .db_entry = LIST_NODE_INIT((r).db_entry), \
    }

#define DB_INIT(d) \
    { \
        .rows = LIST_HEAD_INIT((d).rows), \
    }

static inline void db_entry_init(struct db_entry *e)
{
    *e = (struct db_entry)DB_ENTRY_INIT(*e);
}

static inline void db_row_init(struct db_row *r)
{
    *r = (struct db_row)DB_ROW_INIT(*r);
}

static inline void db_init(struct db *d)
{
    *d = (struct db)DB_INIT(*d);
}

int db_entry_comma_sep(struct db_entry *ent);
int db_read(struct db *db, FILE *file);
int db_write(struct db *db, FILE *file);

void db_entry_clear(struct db_entry *ent);
void db_row_clear(struct db_row *row);
void db_clear(struct db *db);

void db_row_add_string_entry(struct db_row *row, const char *str);
void db_row_add_int_entry(struct db_row *row, int val);

#endif
