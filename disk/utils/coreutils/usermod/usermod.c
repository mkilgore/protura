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
const char *prog_name;

static const char *change_comment = NULL;
static uid_t change_uid = (uid_t)-1;
static gid_t change_gid = (gid_t)-1;
static const char *change_login = NULL;
static const char *change_shell = NULL;
static const char *change_home = NULL;

#define error(cond) \
    do { \
        if (cond) { \
            fprintf(stderr, "%s: Unexpected argument: \"%s\"\n", argv[0], argarg); \
            return 1; \
        } \
    } while (0)


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

    return passwd_db_save(&db);;
}
