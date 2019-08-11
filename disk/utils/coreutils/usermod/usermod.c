// usermod - Modify an existing user
#define UTILITY_NAME "usermod"

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
static const char *usage_str = "Modifying a user in the system.\n";
static const char *arg_desc_str  = "User: A user that exists on the system\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(comment, "comment", 'c', 1, "string", "Change comment field for the user") \
    X(gid, "gid", 'g', 1, "gid", "Change the initial gid for the user") \
    X(uid, "uid", 'u', 1, "uid", "Change the UID for the user") \
    X(login, "login", 'l', 1, "name", "Change the name of the user") \
    X(shell, "shell", 's', 1, "shell", "Change the login shell for the user") \
    X(home, "home", 'h', 1, "directory", "Change the home directory for the user") \
    X(append, "append", 'a', 0, NULL, "Append groups instead of replace, with -G") \
    X(groups, "groups", 'G', 1, "groups", "List of supplementary groups this user belongs too") \
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

static const char *change_comment = NULL;
static uid_t change_uid = (uid_t)-1;
static gid_t change_gid = (gid_t)-1;
static const char *change_login = NULL;
static const char *change_shell = NULL;
static const char *change_home = NULL;
static char *change_groups = NULL;
static int append_groups = 0;

#define error(cond) \
    do { \
        if (cond) { \
            fprintf(stderr, "%s: Unexpected argument: \"%s\"\n", argv[0], argarg); \
            return 1; \
        } \
    } while (0)

static void change_sup_groups(const char *username)
{
    char *next_ptr = NULL;
    char *val;

    if (!append_groups)
        group_db_remove_user(&group_db, username);

    for (val = strtok_r(change_groups, ",", &next_ptr); val; val = strtok_r(NULL, ",", &next_ptr))
        group_add_user(group_db_get_group(&group_db, val), username);
}

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

        case ARG_comment:
            error(change_comment);

            change_comment = argarg;
            break;

        case ARG_uid:
            error(change_uid != (uid_t)-1);

            change_uid = atoi(argarg);
            break;

        case ARG_gid:
            error(change_gid != (gid_t)-1);

            change_uid = atoi(argarg);
            break;

        case ARG_login:
            error(change_login);
            change_login = argarg;
            break;

        case ARG_shell:
            error(change_shell);
            change_shell = argarg;
            break;

        case ARG_home:
            error(change_home);
            change_home = argarg;
            break;

        case ARG_groups:
            error(change_groups);
            change_groups = argarg;
            break;

        case ARG_append:
            error(append_groups);
            append_groups = 1;
            break;

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

    ret = group_db_load(&group_db);
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

    if (change_comment) {
        free(ent->description);
        ent->description = strdupx(change_comment);
    }

    if (change_uid != (uid_t)-1)
        ent->uid = change_uid;

    if (change_gid != (gid_t)-1)
        ent->gid = change_gid;

    if (change_login) {
        free(ent->username);
        ent->username = strdupx(change_login);
    }

    if (change_shell) {
        free(ent->shell);
        ent->shell = strdupx(change_shell);
    }

    if (change_home) {
        free(ent->home_dir);
        ent->home_dir = strdupx(change_home);
    }

    if (change_groups)
        change_sup_groups(ent->username);

    ret = passwd_db_save(&db);
    if (ret)
        fprintf(stderr, "%s: Error writing to /etc/passwd\n", argv[0]);

    ret = group_db_save(&group_db);
    if (ret)
        fprintf(stderr, "%s: Error writing to /etc/group\n", argv[0]);

    return 0;
}
