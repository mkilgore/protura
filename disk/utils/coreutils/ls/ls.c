// ls - list files and directories inside a directory
#define UTILITY_NAME "ls"

#include "common.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "list.h"
#include "columns.h"
#include "arg_parser.h"
#include "db_passwd.h"
#include "db_group.h"

#define DIRMAX 200

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "List Files and directories inside of a directory\n";
static const char *arg_desc_str  = "Files: List of files to provide information on.\n"
                                   "       An empty list means all files in the current directory\n";

#define XARGS \
    X(all, "all", 0, 'a', "Do not ignore entries starting with '.'") \
    X(almost_all, "almost-all", 0, 'A', "All, but '.' and '..' are not listed") \
    X(lng, NULL,  0, 'l', "Use a long listing format") \
    X(dereference, "dereference", 0, 'L', "For symlinks: Show information on referenced file instead") \
    X(inode, "inode", 0, 'i', "Show Inode numbers") \
    X(human, "human-readable", 0, 'h', "Show human-readable sizes") \
    X(size, "size", 0, 's', "Show size of files") \
    X(help, "help", 0, '\0', "Display help") \
    X(version, "version", 0, 'v', "Display version information") \
    X(last, NULL, 0, '\0', NULL)

enum arg_index {
    ARG_EXTRA = ARG_PARSER_EXTRA,
    ARG_ERR = ARG_PARSER_ERR,
    ARG_DONE = ARG_PARSER_DONE,
#define X(enu, id, arg, op, help_text) ARG_##enu,
    XARGS
#undef X
};

static const struct arg ls_args[] = {
#define X(enu, id, arg, op, help_text) [ARG_##enu] = { .lng = id, .shrt = op, .help_txt = help_text, .has_arg = arg },
    XARGS
#undef X
};

enum all_option {
    SHOW_NORMAL,
    SHOW_ALL,
    SHOW_ALMOST_ALL,
};

const char *prog_name;
static struct passwd_db pwdb = PASSWD_DB_INIT(pwdb);
static struct group_db groupdb = GROUP_DB_INIT(groupdb);
static struct util_display display = UTIL_DISPLAY_INIT(display);
static enum all_option show_all = SHOW_NORMAL;
static int long_fmt = 0;
static int show_inode = 0;
static int show_size = 0;
static int dereference_links = 0;
static int human_readable_size = 0;
static int show_usernames = 1;
static int show_groups = 1;

enum file_type {
    TYPE_UNKNOWN,
    TYPE_REG,
    TYPE_DIR,
    TYPE_BLK,
    TYPE_CHR,
    TYPE_LNK,
    TYPE_FIFO,
    TYPE_SOK,
};

static char type_ids[] = {
    [TYPE_UNKNOWN] = 'u',
    [TYPE_REG] = '-',
    [TYPE_DIR] = 'd',
    [TYPE_BLK] = 'b',
    [TYPE_CHR] = 'c',
    [TYPE_LNK] = 'l',
    [TYPE_FIFO] = 'f',
    [TYPE_SOK] = 's',
};

enum file_type mode_to_file_type(mode_t mode)
{
    switch (mode & S_IFMT) {
    case S_IFREG:
        return TYPE_REG;

    case S_IFDIR:
        return TYPE_DIR;

    case S_IFBLK:
        return TYPE_BLK;

    case S_IFCHR:
        return TYPE_CHR;

    case S_IFLNK:
        return TYPE_LNK;

    case S_IFIFO:
        return TYPE_FIFO;

    case S_IFSOCK:
        return TYPE_SOK;
    }

    return TYPE_UNKNOWN;
}

char mode_flag(mode_t mode, mode_t flag, char c)
{
    if (mode & flag)
        return c;

    return '-';
}

static void render_mode_flags(struct util_line *line, mode_t mode)
{
    util_line_printf(line,
            "%c" "%c%c%c" "%c%c%c" "%c%c%c",
            type_ids[mode_to_file_type(mode)],
            mode_flag(mode, S_IRUSR, 'r'), mode_flag(mode, S_IWUSR, 'w'), mode_flag(mode, S_IXUSR, 'x'),
            mode_flag(mode, S_IRGRP, 'r'), mode_flag(mode, S_IWGRP, 'w'), mode_flag(mode, S_IXGRP, 'x'),
            mode_flag(mode, S_IROTH, 'r'), mode_flag(mode, S_IWOTH, 'w'), mode_flag(mode, S_IXOTH, 'x')
            );
}

static void render_size(struct util_line *line, off_t size)
{
    struct util_column *column = util_line_next_column(line);

    column->alignment = UTIL_ALIGN_RIGHT;
    if (!human_readable_size) {
        util_column_printf(column, "%d", (int)size);
        return;
    }

    if (size < 1024) {
        util_column_printf(column, "%d", (int)size);
        return;
    }

    if (size < 1024 * 1024) {
        util_column_printf(column, "%dK", (int)size >> 10);
        return;
    }

    if (size < 1024 * 1024 * 1024) {
        util_column_printf(column, "%dM", (int)size >> 20);
        return;
    }

    util_column_printf(column, "%dG", (int)size >> 30);
}

static void render_user(struct util_line *line, uid_t uid)
{
    struct passwd_entry *ent = NULL;

    if (show_usernames)
        ent = passwd_db_get_uid(&pwdb, uid);

    if (ent)
        util_line_printf(line, "%s", ent->username);
    else
        util_line_printf(line, "%d", (int)uid);
}

static void render_group(struct util_line *line, gid_t gid)
{
    struct group *ent = NULL;

    if (show_usernames)
        ent = group_db_get_gid(&groupdb, gid);

    if (ent)
        util_line_printf(line, "%s", ent->group_name);
    else
        util_line_printf(line, "%d", (int)gid);
}

void list_items(DIR *directory)
{
    struct dirent *item;

    while ((item = readdir(directory))) {
        struct stat st;
        struct util_line *cur_line;

        if (show_all == SHOW_NORMAL && item->d_name[0] == '.')
            continue;

        if (show_all == SHOW_ALMOST_ALL
            && (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0))
            continue;

        if (!dereference_links)
            lstat(item->d_name, &st);
        else
            stat(item->d_name, &st);

        cur_line = util_display_next_line(&display);

        if (show_size)
            render_size(cur_line, st.st_size);

        if (show_inode)
            util_line_printf(cur_line, "%d", (int)st.st_ino);

        if (long_fmt) {
            render_mode_flags(cur_line, st.st_mode);

            render_user(cur_line, st.st_uid);
            render_group(cur_line, st.st_gid);

            if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
                render_size(cur_line, st.st_size);

            if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
                util_line_printf(cur_line, "%d, %d", (int)major(st.st_rdev), (int)minor(st.st_rdev));

            struct util_column *col = util_line_next_column(cur_line);
            col->buf = malloc(100);
            strftime(col->buf, 100, "%b %e %H:%M", gmtime(&st.st_mtime));
        }

        if (!S_ISLNK(st.st_mode)) {
            util_line_strdup(cur_line, item->d_name);
        } else {
            char buf[255];
            readlink(item->d_name, buf, sizeof(buf));

            util_line_printf(cur_line, "%s -> %s", item->d_name, buf);
        }
    }

    util_display_render(&display);
}


int main(int argc, char **argv) {
    size_t dirs = 0, i;
    const char *dirarray[DIRMAX] = { NULL };

    prog_name = argv[0];

    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, ls_args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, ls_args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_all:
            show_all = SHOW_ALL;
            break;

        case ARG_almost_all:
            show_all = SHOW_ALMOST_ALL;
            break;

        case ARG_lng:
            long_fmt = 1;
            break;

        case ARG_dereference:
            dereference_links = 1;
            break;

        case ARG_inode:
            show_inode = 1;
            break;

        case ARG_human:
            human_readable_size = 1;
            break;

        case ARG_size:
            show_size = 1;
            break;

        case ARG_EXTRA:
            if (dirs < sizeof(dirarray)) {
                dirarray[dirs++] = argarg;
            } else {
                fprintf(stderr, "%s: To many directories given to ls\n", argv[0]);
                return 1;
            }
            break;

        default:
            return 0;
        }
    }

    if (dirs == 0)
        dirarray[dirs++] = "./";

    int err = passwd_db_load(&pwdb);
    if (err) {
        fprintf(stderr, "%s: Unable to parse /etc/passwd\n", argv[0]);
        show_usernames = 0;
    }

    err = group_db_load(&groupdb);
    if (err) {
        fprintf(stderr, "%s: Unable to parse /etc/group\n", argv[0]);
        show_groups = 0;
    }

    for (i = 0; i < dirs; i++) {
        DIR *directory = opendir(dirarray[i]);

        if (directory == NULL) {
            perror(*argv);
            return 1;
        }

        if (dirs > 1)
            printf("%s:\n", dirarray[i]);

        list_items(directory);

        if (dirs > 1)
            printf("\n");
    }
}

