// userdel - Delete a user from the system
#define UTILITY_NAME "userdel"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>

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

static void exec_user(struct passwd_entry *user)
{
    gid_t *gids = NULL;
    size_t group_count = 0;
    struct group *group;
    struct group_member *member;

    /* Setup groups first */
    list_foreach_entry(&group_db.group_list, group, entry) {
        list_foreach_entry(&group->member_list, member, entry) {
            if (strcmp(member->username, user->username) == 0) {
                group_count++;
                gids = realloc(gids, group_count * sizeof(*gids));
                gids[group_count - 1] = group->gid;
                break;
            }
        }
    }

    int ret = chdir(user->home_dir);
    if (ret)
        printf("error: chdir: %s\n", strerror(errno));
    else
        setenv("PWD", user->home_dir, 1);

    setenv("HOME", user->home_dir, 1);
    setenv("USER", user->username, 1);

    setgroups(group_count, gids);

    setgid(user->gid);
    setuid(user->uid);

    /* setuid and then execute the shell */
    char **args = (char *[]) { user->shell, NULL };
    execv(user->shell, args);
    exit(1);
}

static char *read_password(const char *prompt)
{
    char *ret = NULL;
    size_t len = 0;
    struct termios old_termios, termios;

    tcgetattr(STDIN_FILENO, &old_termios);
    termios = old_termios;

    termios.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &termios);

    printf("%s", prompt);
    fflush(stdout);

    ssize_t s = getline(&ret, &len, stdin);
    ret[s - 1] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    return ret;
}

static void login(void)
{
    char *username = NULL;
    size_t username_len = 0;
    char *password = NULL;

    while (1) {
        free(password);

        printf("Login: ");
        fflush(stdout);

        ssize_t s = getline(&username, &username_len, stdin);
        username[s - 1] = '\0';

        char *password = read_password("Password: ");
        putchar('\n');

        if (username && password) {
            struct passwd_entry *ent = passwd_db_get_user(&db, username);
            if (!ent) {
                printf("Unknown user: %s\n", username);
                continue;
            }

            if (strcmp(ent->password, password) != 0) {
                printf("Incorrect password\n");
                continue;
            }

            exec_user(ent);
        }
    }
}

int main(int argc, char **argv)
{
    enum arg_index a;
    prog_name = argv[0];

    while ((a = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (a) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            fprintf(stderr, "%s: Unexpected argument: \"%s\"\n", argv[0], argarg);
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

    login();

    return 0;
}
