// userdel - Delete a user from the system
#define UTILITY_NAME "userdel"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"
#include "db_passwd.h"
#include "db_group.h"
#include "file.h"

static const char *arg_str = "[Flags] [User]";
static const char *usage_str = "Delete a user from the system.\n";
static const char *arg_desc_str  = "User: A user that exists on the system\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(force, "force", 'f', 0, NULL, "File db to read") \
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

static struct passwd_db db = PASSWD_DB_INIT(db);
static struct group_db group_db = GROUP_DB_INIT(group_db);
const char *prog_name;

int main(int argc, char **argv)
{
    enum arg_index a;
    const char *user = NULL;

    prog_name = argv[0];

    while ((a= arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (a) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            if (user) {
                fprintf(stderr, "%s: Unexpected argument: \"%s\"\n", argv[0], argarg);
                return 1;
            }

            user = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!user) {
        fprintf(stderr, "%s: Expected username.\n", argv[0]);
        return 1;
    }

    int ret = passwd_db_load(&db);
    if (ret)
        return 1;

    ret = group_db_load(&db);
    if (ret)
        return 1;

    struct passwd_entry *ent;

    list_foreach_entry(&db.pw_list, ent, entry)
        if (ent->username)
            if (strcmp(ent->username, user) == 0)
                break;

    if (list_ptr_is_head(&db.pw_list, &ent->entry)) {
        fprintf(stderr, "%s: Unknown user: \"%s\"\n", argv[0], user);
        return 1;
    }

    list_del(&ent->entry);
    passwd_entry_clear(ent);
    free(ent);

    group_db_remove_user(&group_db, user);

    passwd_db_save(&db);
    group_db_save(&group_db);

    return 0;
}
