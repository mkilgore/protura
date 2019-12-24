// rm - Remove files or directories
#define UTILITY_NAME "rm"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Remove files or directories.\n";
static const char *arg_desc_str  = "Files: A list of files or directories to delete.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(force, "force", 'f', 0, NULL, "Ignore nonexistant files and arguments, never prompt") \
    X(prompt, NULL, 'i', 0, NULL, "Prompt before every removal") \
    X(recursive, "recursive", 'r', 0, NULL, "Remove directories and their contents recursively") \
    X(recursive2, NULL, 'R', 0, NULL, "Remove directories and their contents recursively") \
    X(directory, "dir", 'd', 0, NULL, "Remove empty directories") \
    X(verbose, "verbose", 'v', 0, NULL, "Explain what is being done") \
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

enum dir_rm_type {
    DIR_NONE,
    DIR_EMPTY,
    DIR_RECURSE,
};

static const char *prog;
static enum dir_rm_type dir_action;
static int force = 0;
static int prompt = 0;
static int verbose = 0;

static int yn_prompt(const char *format, ...)
{
    const char *yes_resp[] = {
        "y", "yes", NULL
    };

    char input[50], *str;
    const char **resp;
    va_list lst;

    printf("%s: ", prog);

    va_start(lst, format);
    vprintf(format, lst);
    va_end(lst);

    putchar(' ');

    fgets(input, sizeof(input), stdin);

    str = strchr(input, '\n');
    if (str)
        *str = '\0';

    for (str = input; *str; str++)
        *str = tolower(*str);

    for (resp = yes_resp; *resp; resp++)
        if (strcmp(input, *resp) == 0)
            return 1;

    return 0;
}

#define yn_prompt_cond(...) \
    (!prompt || yn_prompt(__VA_ARGS__))

static int rm_node(const char *path);

static int dir_empty(const char *dirpath)
{
    DIR *dir;
    int ret;
    int pathlen = strlen(dirpath);
    struct dirent *ent;

    dir = opendir(dirpath);

    while ((ent = readdir(dir)) != NULL) {
        char cpy[pathlen + strlen(ent->d_name) + 2];

        if (ent->d_name[0] == '.' &&
                (!ent->d_name[1] || (ent->d_name[1] == '.' && !ent->d_name[2])))
            continue;

        snprintf(cpy, sizeof(cpy), "%s/%s", dirpath, ent->d_name);

        ret = rm_node(cpy);
        if (ret == -1)
            goto close_dir;
    }

    ret = 0;

  close_dir:
    closedir(dir);
    return ret;
}

static int rm_node(const char *path)
{
    struct stat stat;
    const char *name = "regular";

    if (lstat(path, &stat) == -1)
        return -1;

    if (S_ISBLK(stat.st_mode))
        name = "block device";
    else if (S_ISCHR(stat.st_mode))
        name = "character device";
    else if (S_ISFIFO(stat.st_mode))
        name = "fifo";
    else if (S_ISLNK(stat.st_mode))
        name = "symbolic link";

    if (!S_ISDIR(stat.st_mode)) {
        if (!yn_prompt_cond("Remove %s file '%s'? ", name, path))
            return -2;

        return unlink(path);
    }

    if (dir_action == DIR_NONE) {
        errno = EISDIR;
        return -1;
    }

    if (dir_action == DIR_RECURSE && yn_prompt_cond("Decend into directory '%s'? ", path))
        if (dir_empty(path) == -1)
            return -1;

    if (!yn_prompt_cond("Remove directory '%s'? ", path))
        return -2;

    return rmdir(path);
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int err;

    prog = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_force:
            force = 1;
            break;

        case ARG_recursive:
        case ARG_recursive2:
            dir_action = DIR_RECURSE;
            break;

        case ARG_directory:
            dir_action = DIR_EMPTY;
            break;

        case ARG_verbose:
            verbose = 1;
            break;

        case ARG_prompt:
            prompt = 1;
            break;

        case ARG_EXTRA:
            if ((err = rm_node(argarg)) == -1 && !force) {
                printf("%s: Cannot remove '%s': %s\n", prog, argarg, strerror(errno));
                return 1;
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    return 0;
}

