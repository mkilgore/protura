// chown - change file owner
#define UTILITY_NAME "chown"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"
#include "db_passwd.h"
#include "db_group.h"

static const char *arg_str = "[Flags] owner[:group] [Files]";
static const char *usage_str = "Change owner and optionally group on a file or files.\n";
static const char *arg_desc_str = "Files: List of files to apply ownership too.\n";

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

static struct passwd_db pwdb = PASSWD_DB_INIT(pwdb);
static struct group_db groupdb = GROUP_DB_INIT(groupdb);

const char *prog_name;

static int parse_ownership(char *str, uid_t *uid, gid_t *gid)
{
    char *user = str;
    char *group = NULL;
    char *c = strchr(str, ':');

    if (c) {
        /* Replace colon with NUL, creating separate username and group strings */
        *c = '\0';
        group = c + 1;
    }

    if (*user) {
        char *endp = NULL;
        uid_t value = strtol(user, &endp, 10);

        if (!*endp) {
            *uid = value;
        } else {
            int err = passwd_db_load(&pwdb);
            if (err) {
                fprintf(stderr, "%s: Unable to parse /etc/passwd\n", prog_name);
                return 1;
            }

            struct passwd_entry *ent = passwd_db_get_user(&pwdb, user);
            if (!ent) {
                fprintf(stderr, "%s: Unknown owner: \"%s\"\n", prog_name, user);
                return 1;
            }

            *uid = ent->uid;
        }
    } else {
        *uid = (uid_t)-1;
    }

    if (group && *group) {
        char *endp = NULL;
        uid_t value = strtol(group, &endp, 10);

        if (!*endp) {
            *gid = value;
        } else {
            int err = group_db_load(&groupdb);
            if (err) {
                fprintf(stderr, "%s: Unable to parse /etc/group\n", prog_name);
                return 1;
            }

            struct group *ent = group_db_get_group(&groupdb, group);
            if (!ent) {
                fprintf(stderr, "%s: Unknown group: \"%s\"\n", prog_name, group);
                return 1;
            }

            *gid = ent->gid;
        }
    } else if (group) {
        *gid = getgid();
    } else {
        *gid = (gid_t)-1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int have_ownership = 0;
    uid_t uid = 0;
    gid_t gid = 0;

    prog_name = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            if (!have_ownership) {
                int err = parse_ownership(argarg, &uid, &gid);
                if (err)
                    return 1;

                have_ownership = 1;
            } else {
                printf("Applying UID: %d, GID: %d\n", uid, gid);
                int err = chown(argarg, uid, gid);
                if (err) {
                    fprintf(stderr, "%s: %s: %s\n", prog_name, argarg, strerror(errno));
                    return 1;
                }
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    return 0;
}
