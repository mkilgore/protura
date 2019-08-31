// uname - Print system information
#define UTILITY_NAME "uname"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags]";
static const char *usage_str = "Display system information.\n";
static const char *arg_desc_str  = "";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", '\0', 0, NULL, "Display version information") \
    X(all, "all", 'a', 0, NULL, "Display all information") \
    X(kernel, "kernel-name", 's', 0, NULL, "Display kernel name") \
    X(nodename, "nodename", 'n', 0, NULL, "Display network node hostname") \
    X(kernel_rel, "kernel-release", 'r', 0, NULL, "Display kernel release") \
    X(kernel_ver, "kernel-version", 'v', 0, NULL, "Display kernel version") \
    X(machine, "machine", 'm', 0, NULL, "Display machine hardware name") \
    X(os, "operating-system", 'o', 0, NULL, "Display the Operation System name") \
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

int print_kernel_name = 0;
int print_node = 0;
int print_release = 0;
int print_version = 0;
int print_machine = 0;
int print_os = 0;

struct utsname utsname;

static void print_with_space(char *s, int with_space)
{
    if (with_space)
        putchar(' ');

    printf("%s", s);
}

int main(int argc, char **argv)
{
    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            printf("%s: Unexpected Argument \"%s\"\n", argv[0], argarg);
            return 1;

        case ARG_kernel:
            print_kernel_name = 1;
            break;

        case ARG_nodename:
            print_node = 1;
            break;

        case ARG_kernel_rel:
            print_release = 1;
            break;

        case ARG_kernel_ver:
            print_version = 1;
            break;

        case ARG_machine:
            print_machine = 1;
            break;

        case ARG_os:
            print_os = 1;
            break;

        case ARG_all:
            print_kernel_name = 1;
            print_node = 1;
            print_release = 1;
            print_version = 1;
            print_machine = 1;
            print_os = 1;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!print_kernel_name && !print_node && !print_release && !print_version && !print_machine && !print_os)
        print_kernel_name = 1;

    int add_space = 0;

    uname(&utsname);

    if (print_kernel_name) {
        print_with_space(utsname.sysname, add_space);
        add_space = 1;
    }

    if (print_node) {
        print_with_space(utsname.nodename, add_space);
        add_space = 1;
    }

    if (print_release) {
        print_with_space(utsname.release, add_space);
        add_space = 1;
    }

    if (print_version) {
        print_with_space(utsname.version, add_space);
        add_space = 1;
    }

    if (print_machine) {
        print_with_space(utsname.machine, add_space);
        add_space = 1;
    }

    putchar('\n');

    return 0;
}

