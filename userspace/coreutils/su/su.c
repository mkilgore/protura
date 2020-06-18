// su - switch user
#define UTILITY_NAME "su"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <unistd.h>

#include "arg_parser.h"
#include "db_passwd.h"
#include "db_group.h"
#include "login_common.h"

static const char *arg_str = "[Flags] mode [Files]";
static const char *usage_str = "Change permissions on a file or files.\n";
static const char *arg_desc_str = "Files: List of files to apply permissions too.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(login, "login", 'l', 0, NULL, "Start a login shell") \
    X(command, "command", 'c', 1, "command", "Command to run in shell") \
    X(shell, "shell", 's', 1, "command", "Shell executable to run instead of default") \
    X(env, "preserve-environment", 'p', 0, NULL, "Preserve environment") \
    X(group, "group", 'g', 1, "Group", "Set the primary group") \
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

#define DEFAULT_USER  "root"

#define DEFAULT_PATH "/bin:/usr/bin"

static struct passwd_db pwdb = PASSWD_DB_INIT(pwdb);
static struct group_db groupdb = GROUP_DB_INIT(groupdb);

const char *prog_name;
static int use_login = 0;
static int change_env = 1;
static const char *command = NULL;
static const char *shell_exe = NULL;
static const char *group = NULL;
static const char *user = NULL;

static struct passwd_entry *user_ent;
static struct group *group_ent;

void change_environment(void)
{
    if (use_login) {
        const char *term = getenv("TERM");
        term = strdupx(term);

        environ = malloc(6 * sizeof(char *));
        environ[0] = NULL;

        setenv("PATH", DEFAULT_PATH, 1);
        setenv("USER", user_ent->username, 1);
        setenv("HOME", user_ent->home_dir, 1);
        setenv("SHELL", user_ent->shell, 1);
        if (term)
            setenv("TERM", term, 1);
    } else if (change_env) {
        setenv("HOME", user_ent->home_dir, 1);
        setenv("SHELL", user_ent->shell, 1);

        if (user_ent->uid)
            setenv("USER", user_ent->username, 1);
    }
}

int auth_user(void)
{
    int tries;
    for (tries = 0; tries < 3; tries++) {
        char *password = read_password("Password: ");

        if (strcmp(user_ent->password, password) == 0)
            return 0;

        printf("%s: Incorrect password!\n", prog_name);
    }

    return 1;
}

void start_shell(int extra_args, char **argv)
{
    /* shell -c command extra_args */
    const char **args = malloc((4 + extra_args) * sizeof(*args));
    const char **current = args;

    if (use_login) {
        /* Prepend a '-' to the first argument to tell the shell this is a login shell */
        size_t new_len = strlen(shell_exe) + 2;
        char *dash_shell = malloc(new_len);
        snprintf(dash_shell, new_len, "-%s", shell_exe);

        *current++ = dash_shell;
    } else {
        *current++ = shell_exe;
    }

    if (command) {
        *current++ = "-c";
        *current++ = command;
    }

    int i;
    for (i = 0; i < extra_args; i++)
        *current++ = argv[i];

    *current++ = NULL;

    int err = execv(shell_exe, (char *const *)args);
    if (err) {
        printf("%s: execve: %s\n", prog_name, strerror(errno));
        exit(1);
    }
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int cmd_arguments_start = -1;

    prog_name = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_login:
            use_login = 1;
            break;

        case ARG_command:
            command = argarg;
            break;

        case ARG_shell:
            shell_exe = argarg;
            break;

        case ARG_env:
            change_env = 0;
            break;

        case ARG_group:
            group = argarg;
            break;

        case ARG_EXTRA:
            if (strcmp(argarg, "-") == 0) {
                use_login = 1;
            } else if (!user) {
                user = argarg;
            } else if (cmd_arguments_start == -1) {
                cmd_arguments_start = current_arg;
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (use_login && !change_env) {
        printf("%s: --login and --preserve-environment cannot be used together\n", argv[0]);
        return 1;
    }

    int err = passwd_db_load(&pwdb);
    if (err) {
        fprintf(stderr, "%s: Unable to parse /etc/passwd\n", argv[0]);
        return 1;
    }

    err = group_db_load(&groupdb);
    if (err) {
        fprintf(stderr, "%s: Unable to parse /etc/group\n", argv[0]);
        return 1;
    }

    if (user)
        user_ent = passwd_db_get_user(&pwdb, user);
    else
        user_ent = passwd_db_get_user(&pwdb, DEFAULT_USER);

    if (!user_ent) {
        printf("%s: Unknown user: \"%s\"\n", prog_name, user? user: DEFAULT_USER);
        return 1;
    }

    if (!shell_exe)
        shell_exe = user_ent->shell;

    if (group) {
        group_ent = group_db_get_group(&groupdb, group);
        if (!group_ent) {
            printf("%s: Unknown group: \"%s\"\n", prog_name, group);
            return 1;
        }
    }

    if (auth_user())
        return 1;

    change_environment();

    if (group_ent)
        user_ent->gid = group_ent->gid;

    swap_user(user_ent, &groupdb);

    if (use_login) {
        err = chdir(user_ent->home_dir);
        if (err) {
            printf("%s: chdir: %s\n", prog_name, strerror(errno));
            return 1;
        }
    }

    if (cmd_arguments_start != -1)
        start_shell(argc - cmd_arguments_start, argv + cmd_arguments_start);
    else
        start_shell(0, NULL);

    /* start_shell() should not return */
    return 1;
}
