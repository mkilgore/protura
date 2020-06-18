
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>

#include "db_passwd.h"
#include "db_group.h"
#include "login_common.h"

void swap_user(struct passwd_entry *user, struct group_db *group_db)
{
    gid_t *gids = NULL;
    size_t group_count = 0;
    struct group *group;
    struct group_member *member;

    /* Setup groups first */
    list_foreach_entry(&group_db->group_list, group, entry) {
        list_foreach_entry(&group->member_list, member, entry) {
            if (strcmp(member->username, user->username) == 0) {
                group_count++;
                gids = realloc(gids, group_count * sizeof(*gids));
                gids[group_count - 1] = group->gid;
                break;
            }
        }
    }

    setgroups(group_count, gids);

    setgid(user->gid);
    setuid(user->uid);
}

char *read_password(const char *prompt)
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

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);

    putchar('\n');

    if (s == -1)
        return NULL;

    if (s > 0)
        ret[s - 1] = '\0';

    return ret;
}
