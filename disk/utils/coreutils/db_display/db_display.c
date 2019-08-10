// db_display - Output the first characters in a file
#define UTILITY_NAME "db_display"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"
#include "db.h"
#include "file.h"

static const char *arg_str = "[Flags] [File]";
static const char *usage_str = "display db\n";
static const char *arg_desc_str  = "File: File to parse.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(file, "file", 'f', 0, NULL, "File db to read") \
    X(last, NULL, '\0', 0, NULL, NULL)

enum arg_index {
  ARG_EXTRA = ARG_PARSER_EXTRA,
  ARG_ERR = ARG_PARSER_ERR,
  ARG_DONE = ARG_PARSER_DONE,
#define X(enu, ...) ARG_ENUM(enu)
  XARGS
#undef X
};

static const struct arg args[] = {
#define X(...) CREATE_ARG(__VA_ARGS__)
  XARGS
#undef X
};

const char *prog_name;
static struct db db = DB_INIT(db);

static inline void db_disp(FILE *f)
{
    struct db_row *row;
    struct db_entry *ent;

    printf("Displaying file:\n");
    list_foreach_entry(&db.rows, row, db_entry) {
        printf("Row:\n");
        list_foreach_entry(&row->entry_list, ent, row_entry)
            printf("  Ent: %s\n", ent->contents);
    }
}

int main(int argc, char **argv)
{
    struct db_row *row = malloc(sizeof(*row));
    struct db_entry *ent;
    enum arg_index ret;
    const char *file_sel = "/etc/passwd";
    FILE *file;
    int add = 0;
    db_row_init(row);

    prog_name = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_file:
            file_sel = argarg;
            break;

        case ARG_EXTRA:
            add = 1;

            ent = malloc(sizeof(*ent));
            db_entry_init(ent);

            ent->contents = strdup(argarg);
            list_add_tail(&row->entry_list, &ent->row_entry);
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    file = fopen_with_dash(file_sel, "r+");
    if (!file) {
        perror(argarg);
        return 1;
    }

    db_read(&db, file);

    if (add)
        list_add_tail(&db.rows, &row->db_entry);

    db_disp(file);

    if (add)
        db_write(&db, file);

    fclose_with_dash(file);

    return 0;
}
