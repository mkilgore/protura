// ls - list files and directories inside a directory
#define UTILITY_NAME "ls"

#include "common.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
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
    X(lng, NULL,  0, 'l', "Use a long listing format") \
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

static int show_all = 0;
static int long_fmt = 0;

void list_items(DIR *directory) {
    struct dirent *item;

    while ((item = readdir(directory))) {
        struct stat st;

        if (!show_all && item->d_name[0] == '.')
            continue;

        if (!long_fmt) {
            puts(item->d_name);
            continue;
        }

        stat(item->d_name, &st);

        if (S_ISDIR(st.st_mode)) {
            printf("d\t%d\t%ld\t%s\n", (int)st.st_ino, (long)st.st_size, item->d_name);
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            printf("-\t%d\t%ld\t%s\n", (int)st.st_ino, (long)st.st_size, item->d_name);
            continue;
        }

        if (S_ISCHR(st.st_mode)) {
            printf("c\t%d\t%d, %d\t%s\n", (int)st.st_ino, (int)major(st.st_rdev), (int)minor(st.st_rdev), item->d_name);
            continue;
        }

        if (S_ISBLK(st.st_mode)) {
            printf("b\t%d\t%d, %d\t%s\n", (int)st.st_ino, (int)major(st.st_rdev), (int)minor(st.st_rdev), item->d_name);
            continue;
        }

        if (S_ISLNK(st.st_mode)) {
            printf("l\t%d\t%ld\t%s\n", (int)st.st_ino, (long)st.st_size, item->d_name);
            continue;
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
            show_all = 1;
            break;

        case ARG_lng:
            long_fmt = 1;
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

