// ls - list files and directories inside a directory
#define UTILITY_NAME "ls"

#include "common.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "arg_parser.h"

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
    X(help, "help", 0, 'h', "Display help") \
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

static enum all_option show_all = SHOW_NORMAL;
static int long_fmt = 0;
static int dereference_links = 0;

static const char *type_ids[] = {
    [S_IFREG] = "r",
    [S_IFDIR] = "d",
    [S_IFBLK] = "b",
    [S_IFCHR] = "c",
    [S_IFLNK] = "l",
    [S_IFIFO] = "f",
};

void list_items(DIR *directory)
{
    struct dirent *item;
    int column_count;
    char columns[10][20];
    int i;

    while ((item = readdir(directory))) {
        struct stat st;

        if (show_all == SHOW_NORMAL && item->d_name[0] == '.')
            continue;

        if (show_all == SHOW_ALMOST_ALL
            && (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0))
            continue;

        if (!long_fmt) {
            puts(item->d_name);
            continue;
        }

        if (!dereference_links)
            lstat(item->d_name, &st);
        else
            stat(item->d_name, &st);

        column_count = 0;

        snprintf(columns[column_count], sizeof(columns[column_count]), "%s", type_ids[st.st_mode & S_IFMT]);
        column_count++;

        snprintf(columns[column_count], sizeof(columns[column_count]), "%d", (int)st.st_ino);
        column_count++;

        if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
            snprintf(columns[column_count], sizeof(columns[column_count]), "%d", (int)st.st_size);
            column_count++;
        }

        if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)){
            snprintf(columns[column_count], sizeof(columns[column_count]), "%d, %d", (int)major(st.st_rdev), (int)minor(st.st_rdev));
            column_count++;
        }

        strftime(columns[column_count], sizeof(columns[column_count]), "%D %T", gmtime(&st.st_mtime));
        column_count++;

        for (i = 0; i < column_count; i++)
            printf("%s\t", columns[i]);

        if (!S_ISLNK(st.st_mode)) {
            printf("%s\n", item->d_name);
        } else {
            char buf[255];
            readlink(item->d_name, buf, sizeof(buf));

            printf("%s -> %s\n", item->d_name, buf);
        }
    }
}


int main(int argc, char **argv) {
    int dirs = 0, i;
    const char *dirarray[DIRMAX] = { NULL };

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

