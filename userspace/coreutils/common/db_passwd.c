
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "db.h"
#include "db_passwd.h"

#define PASSWD_FILE "/etc/passwd"

struct passwd_entry *passwd_db_get_uid(struct passwd_db *db, uid_t uid)
{
    struct passwd_entry *ent;

    list_foreach_entry(&db->pw_list, ent, entry)
        if (ent->uid == uid)
            return ent;

    return NULL;
}

struct passwd_entry *passwd_db_get_user(struct passwd_db *db, const char *user)
{
    struct passwd_entry *ent;

    list_foreach_entry(&db->pw_list, ent, entry)
        if (ent->username)
            if (strcmp(ent->username, user) == 0)
                return ent;

    return NULL;
}

struct passwd_entry *db_row_to_pw_entry(struct db_row *row)
{
    struct passwd_entry *pw_ent = malloc(sizeof(*pw_ent));
    struct db_entry *entry;
    int i = 0;

    passwd_entry_init(pw_ent);

    list_foreach_entry(&row->entry_list, entry, row_entry) {
        switch (i++) {
        case 0:
            pw_ent->username = strdupx(entry->contents);
            break;

        case 1:
            pw_ent->password = strdupx(entry->contents);
            break;

        case 2:
            pw_ent->uid = atoi(entry->contents);
            break;

        case 3:
            pw_ent->gid = atoi(entry->contents);
            break;

        case 4:
            pw_ent->description = strdupx(entry->contents);
            break;

        case 5:
            pw_ent->home_dir = strdupx(entry->contents);
            break;

        case 6:
            pw_ent->shell = strdupx(entry->contents);
            break;
        }
    }

    return pw_ent;
}

struct db_row *pw_entry_to_db_row(struct passwd_entry *pw)
{
    struct db_row *row = malloc(sizeof(*row));
    db_row_init(row);

    db_row_add_string_entry(row, pw->username);
    db_row_add_string_entry(row, pw->password);
    db_row_add_int_entry(row, pw->uid);
    db_row_add_int_entry(row, pw->gid);
    db_row_add_string_entry(row, pw->description);
    db_row_add_string_entry(row, pw->home_dir);
    db_row_add_string_entry(row, pw->shell);

    return row;
}

void passwd_entry_clear(struct passwd_entry *ent)
{
    free(ent->username);
    free(ent->password);
    free(ent->description);
    free(ent->home_dir);
    free(ent->shell);
}

void passwd_db_clear(struct passwd_db *pw)
{
    struct passwd_entry *ent;

    list_foreach_take_entry(&pw->pw_list, ent, entry) {
        passwd_entry_clear(ent);
        free(ent);
    }
}

int passwd_db_load(struct passwd_db *pw)
{
    struct db db;
    struct db_row *row;
    FILE *file = NULL;

    db_init(&db);
    db.column_count = 6;

    if ((file = fopen(PASSWD_FILE, "r")) == NULL) {
        fprintf(stderr, "%s: Error opening passwd file: \"%s\"\n", prog_name, PASSWD_FILE);
        return 1;
    }

    db_read(&db, file);

    list_foreach_entry(&db.rows, row, db_entry) {
        struct passwd_entry *entry = db_row_to_pw_entry(row);
        list_add_tail(&pw->pw_list, &entry->entry);
    }

    if (file)
        fclose(file);

    db_clear(&db);

    return 0;
}

int passwd_db_save(const struct passwd_db *pw)
{
    struct db db;
    struct passwd_entry *pw_ent;
    FILE *file = NULL;
    int ret = 0;

    db_init(&db);

    list_foreach_entry(&pw->pw_list, pw_ent, entry) {
        struct db_row *row = pw_entry_to_db_row(pw_ent);
        list_add_tail(&db.rows, &row->db_entry);
    }

    if ((file = fopen(PASSWD_FILE, "w")) == NULL) {
        fprintf(stderr, "%s: Error opening passwd file: \"%s\"\n", prog_name, PASSWD_FILE);
        ret = 1;
        goto cleanup;
    }

    ret = db_write(&db, file);

  cleanup:
    if (file)
        fclose(file);

    db_clear(&db);
    return ret;
}
