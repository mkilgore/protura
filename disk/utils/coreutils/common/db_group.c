
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "db.h"
#include "db_group.h"

#define GROUP_FILE "/etc/group"

void group_db_remove_user(struct group_db *db, const char *user)
{
    struct group *group;

    list_foreach_entry(&db->group_list, group, entry) {
        struct group_member *member;

        list_foreach_entry(&group->member_list, member, entry)
            if (strcmp(member->username, user) == 0)
                break;

        if (!list_ptr_is_head(&group->member_list, &member->entry)) {
            list_del(&member->entry);
            group_member_clear(member);
            free(member);
        }
    }
}

void group_add_user(struct group *group, const char *user)
{
    struct group_member *member;

    list_foreach_entry(&group->member_list, member, entry)
        if (strcmp(member->username, user) == 0)
            return;

    member = malloc(sizeof(*member));
    group_member_init(member);
    member->username = strdupx(user);

    list_add_tail(&group->member_list, &member->entry);
}

struct group *group_db_get_group(struct group_db *db, const char *name)
{
    struct group *group;

    list_foreach_entry(&db->group_list, group, entry)
        if (strcmp(group->group_name, name) == 0)
            return group;

    return NULL;
}

struct group *group_db_get_gid(struct group_db *db, gid_t gid)
{
    struct group *group;

    list_foreach_entry(&db->group_list, group, entry)
        if (group->gid == gid)
            return group;

    return NULL;
}

static struct group_member *group_member_new(const char *mem)
{
    struct group_member *member = malloc(sizeof(*member));
    group_member_init(member);
    member->username = strdupx(mem);
    return member;
}

static struct group_member *str_to_members(list_head_t *head, const char *tmp_str)
{
    struct group_member *member;
    char *str = strdupx(tmp_str);
    char *val;
    char *next_ptr = NULL;

    for (val = strtok_r(str, ",", &next_ptr); val; val = strtok_r(NULL, ",", &next_ptr)) {
        member = group_member_new(val);
        list_add_tail(head, &member->entry);
    }

    free(str);

    return member;
}

static struct group *db_row_to_group(struct db_row *row)
{
    struct group *group = malloc(sizeof(*group));
    struct db_entry *entry;
    int i = 0;

    group_init(group);

    list_foreach_entry(&row->entry_list, entry, row_entry) {
        switch (i++) {
        case 0:
            group->group_name = strdupx(entry->contents);
            break;

        case 1:
            group->password = strdupx(entry->contents);
            break;

        case 2:
            group->gid = atoi(entry->contents);
            break;

        case 3:
            str_to_members(&group->member_list, entry->contents);
            break;
        }
    }

    return group;
}

struct db_row *group_to_db_row(struct group *group)
{
    struct db_row *row = malloc(sizeof(*row));
    db_row_init(row);

    db_row_add_string_entry(row, group->group_name);
    db_row_add_string_entry(row, group->password);
    db_row_add_int_entry(row, group->gid);

    char *users = NULL;
    size_t users_len = 0;
    struct group_member *member;

    list_foreach_entry(&group->member_list, member, entry) {
        if (!users) {
            users = strdup(member->username);
            users_len = strlen(users);
        } else {
            users_len = users_len + 1 + strlen(member->username);

            users = realloc(users, users_len + 1);
            strcat(users, ",");
            strcat(users, member->username);
        }
    }

    if (users) {
        db_row_add_string_entry(row, users);
        free(users);
    } else {
        db_row_add_string_entry(row, "");
    }

    return row;
}

int group_db_load(struct group_db *group_db)
{
    struct db db;
    struct db_row *row;
    FILE *file = NULL;

    db_init(&db);
    db.column_count = 4;

    if ((file = fopen(GROUP_FILE, "r")) == NULL) {
        fprintf(stderr, "%s: Error opening group file: \"%s\"\n", prog_name, GROUP_FILE);
        return 1;
    }

    db_read(&db, file);

    list_foreach_entry(&db.rows, row, db_entry) {
        struct group *entry = db_row_to_group(row);
        list_add_tail(&group_db->group_list, &entry->entry);
    }

    if (file)
        fclose(file);

    db_clear(&db);

    return 0;
}

int group_db_save(const struct group_db *group_db)
{
    struct db db;
    struct group *group;
    FILE *file = NULL;
    int ret = 0;

    db_init(&db);
    db.column_count = 4;

    list_foreach_entry(&group_db->group_list, group, entry) {
        struct db_row *row = group_to_db_row(group);
        list_add_tail(&db.rows, &row->db_entry);
    }

    if ((file = fopen(GROUP_FILE, "w")) == NULL) {
        fprintf(stderr, "%s: Error opening group file: \"%s\"\n", prog_name, GROUP_FILE);
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

void group_member_clear(struct group_member *member)
{
    free(member->username);
}

void group_clear(struct group *group)
{
    struct group_member *member;

    free(group->group_name);
    free(group->password);

    list_foreach_take_entry(&group->member_list, member, entry) {
        group_member_clear(member);
        free(member);
    }
}

void group_db_clear(struct group_db *db)
{
    struct group *group;

    list_foreach_take_entry(&db->group_list, group, entry) {
        group_clear(group);
        free(group);
    }
}
