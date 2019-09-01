// id - Display informatino about a user
#define UTILITY_NAME "id"

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
static const char *usage_str = "Display information aboutt a user.\n";
static const char *arg_desc_str  = "User: A user that exists on the system, or the current user\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(group, "group", 'g', 0, NULL, "Display only the effective group ID") \
    X(groups, "groups", 'G', 0, NULL, "Display all group IDs") \
    X(name, "name", 'n', 0, NULL, "Display names instead of IDs") \
    X(real, "real", 'r', 0, NULL, "Use the real IDs instead of effective IDs") \
    X(user, "user", 'u', 0, NULL, "Display the effective user ID") \
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

enum print_option {
    PRINT_ALL,
    PRINT_GID,
    PRINT_GROUPS,
    PRINT_USER,
};

const char *prog_name;
static struct passwd_db db = PASSWD_DB_INIT(db);
static struct group_db group_db = GROUP_DB_INIT(group_db);
static enum print_option option;
static int print_names = 0;
static int print_real_ids = 0;

static uid_t user_uid;
static gid_t user_gid;
static int sub_gids_count;
static gid_t *sub_gids = NULL;

static void display_gid(gid_t gid)
{
    struct group *ent = NULL;

    if (print_names)
        ent = group_db_get_gid(&group_db, gid);

    if (ent && option == PRINT_ALL)
        printf("%d(%s)", gid, ent->group_name);
    else if (ent)
        printf("%s", ent->group_name);
    else
        printf("%d", gid);
}

static void display_uid(uid_t uid)
{
    struct passwd_entry *ent = NULL;

    if (print_names)
        ent = passwd_db_get_uid(&db, uid);

    if (ent && option == PRINT_ALL)
        printf("%d(%s)", uid, ent->username);
    else if (ent)
        printf("%s", ent->username);
    else
        printf("%d", uid);
}

static void print_all_ids(void)
{
    printf("uid=");
    display_uid(user_uid);
    printf(" gid=");
    display_gid(user_gid);
    printf(" groups=");

    int i;
    for (i = 0; i < sub_gids_count; i++) {
        if (i != 0)
            putchar(',');
        display_gid(sub_gids[i]);
    }

    putchar('\n');
}

static void print_gid(void)
{
    display_gid(user_gid);
    putchar('\n');
}

static void print_groups(void)
{
    int i;
    for (i = 0; i < sub_gids_count; i++) {
        if (i != 0)
            putchar(' ');
        display_gid(sub_gids[i]);
    }
    putchar('\n');
}

static void print_user(void)
{
    display_uid(user_uid);
    putchar('\n');
}

static void (*display_funcs[]) (void) = {
    [PRINT_ALL] = print_all_ids,
    [PRINT_GID] = print_gid,
    [PRINT_GROUPS] = print_groups,
    [PRINT_USER] = print_user,
};

static void load_uid(const char *username)
{
    if (!username) {
        if (print_real_ids)
            user_uid = getuid();
        else
            user_uid = geteuid();
    }
}

static void load_gid(const char *username)
{
    if (!username) {
        if (print_real_ids)
            user_gid = getgid();
        else
            user_gid = getegid();
    }
}

static void load_sub_gids(const char *username)
{
    if (!username) {
        sub_gids_count = getgroups(0, NULL);
        sub_gids = malloc(sub_gids_count * sizeof(*sub_gids));

        int ret = getgroups(sub_gids_count, sub_gids);
        if (ret == -1)
            perror("getgroups");
    }
}

#define arg_check() \
    do { \
        if (option != PRINT_ALL) { \
            fprintf(stderr, "%s: Arguments -ugG are mutually exclusive\n", prog_name); \
            return 1; \
        } \
    } while (0)

int main(int argc, char **argv)
{
    const char *user = NULL;
    enum arg_index a;

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

        case ARG_group:
            arg_check();
            option = PRINT_GID;
            break;

        case ARG_groups:
            arg_check();
            option = PRINT_GROUPS;
            break;

        case ARG_user:
            arg_check();
            option = PRINT_USER;
            break;

        case ARG_real:
            print_real_ids = 1;
            break;

        case ARG_name:
            print_names = 1;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    int ret = passwd_db_load(&db);
    if (ret)
        return 1;

    ret = group_db_load(&group_db);
    if (ret)
        return 1;

    if (option == PRINT_ALL || option == PRINT_USER)
        load_uid(user);

    if (option == PRINT_ALL || option == PRINT_GID)
        load_gid(user);

    if (option == PRINT_ALL || option == PRINT_GROUPS)
        load_sub_gids(user);

    (display_funcs[option]) ();

    return 0;
}
