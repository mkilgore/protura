// login - Starts a session as an authenticated user
#define UTILITY_NAME "login"

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
#include "login_common.h"

static const char *arg_str = "";
static const char *usage_str = "Authenticates and starts a session as a user.\n";
static const char *arg_desc_str  = "";

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
    swap_user(user, &group_db);

    int ret = chdir(user->home_dir);
    if (ret)
        printf("error: chdir: %s\n", strerror(errno));
    else
        setenv("PWD", user->home_dir, 1);

    setenv("HOME", user->home_dir, 1);
    setenv("USER", user->username, 1);

    /* Prepend a '-' to the first argument to tell the shell this is a login shell */
    size_t new_len = strlen(user->shell) + 2;
    char *dash_shell = malloc(new_len);
    snprintf(dash_shell, new_len, "-%s", user->shell);

    /* setuid and then execute the shell */
    char **args = (char *[]) { dash_shell, NULL };
    execv(user->shell, args);
    exit(1);
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

        if (username && password) {
            struct passwd_entry *ent = passwd_db_get_user(&db, username);
            if (!ent) {
                printf("Unknown user: %s\n\n", username);
                sleep(2);
                continue;
            }

            if (strcmp(ent->password, password) != 0) {
                printf("Incorrect password\n");
                sleep(2);
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
