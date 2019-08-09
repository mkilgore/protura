// ps - List current OS processes
#define UTILITY_NAME "ps"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <protura/fs/fdset.h>
#include <protura/task_api.h>

#include "columns.h"
#include "arg_parser.h"

#define TASK_API_FILE "/proc/task_api"

#define TASK_MAX 200

static const char *arg_str = "[Flags]";
static const char *usage_str = "List the current OS processes.\n";
static const char *arg_desc_str  = "";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(lng, "long", 'l', 0, NULL, "Long format") \
    X(signals, "signal", 's', 0, NULL, "Display signal informatoin") \
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

enum ps_display {
    PS_NORMAL,
    PS_LIST,
    PS_SIGNAL,
};

static const char *task_state_strs[] = {
    [TASK_API_NONE] = "none",
    [TASK_API_SLEEPING] = "sleep",
    [TASK_API_INTR_SLEEPING] = "isleep",
    [TASK_API_RUNNING] = "running",
    [TASK_API_STOPPED] = "stopped",
    [TASK_API_ZOMBIE] = "zombie",
};

static struct util_display display = UTIL_DISPLAY_INIT(display);
static struct task_api_info tinfo[TASK_MAX];
static int task_count;
static enum ps_display display_choice = PS_NORMAL;

static void print_list_format(void)
{
    struct task_api_info *t, *end = tinfo + task_count;

    struct util_line *header = util_display_next_line(&display);
    util_line_strdup(header, "PID");
    util_line_strdup(header, "PPID");
    util_line_strdup(header, "PGRP");
    util_line_strdup(header, "UID");
    util_line_strdup(header, "GID");
    util_line_strdup(header, "TTY");
    util_line_strdup(header, "SID");
    util_line_strdup(header, "STATE");
    util_line_strdup(header, "CMD");

    for (t = tinfo; t != end; t++) {
        struct util_line *line = util_display_next_line(&display);

        util_line_printf_ar(line, "%d", t->pid);
        util_line_printf_ar(line, "%d", t->ppid);
        util_line_printf_ar(line, "%d", t->pgid);
        util_line_printf_ar(line, "%d", t->uid);
        util_line_printf_ar(line, "%d", t->gid);
        util_line_printf(line, "%s", (t->has_tty)? t->tty: "?");
        util_line_printf_ar(line, "%d", t->sid);
        util_line_printf(line, "%s", task_state_strs[t->state]);

        if (t->is_kernel)
            util_line_printf(line, "[%s]", t->name);
        else
            util_line_printf(line, "%s", t->name);
    }

    util_display_render(&display);
}

static void print_signal_format(void)
{
    struct task_api_info *t, *end = tinfo + task_count;

    struct util_line *header = util_display_next_line(&display);
    util_line_strdup(header, "PID");
    util_line_strdup(header, "PENDING");
    util_line_strdup(header, "BLOCKED");
    util_line_strdup(header, "CMD");

    for (t = tinfo; t != end; t++) {
        struct util_line *line = util_display_next_line(&display);

        util_line_printf_ar(line, "%d", t->pid);
        util_line_printf(line, "%08x", t->sig_pending);
        util_line_printf(line, "%08x", t->sig_blocked);

        if (t->is_kernel)
            util_line_printf(line, "[%s]", t->name);
        else
            util_line_printf(line, "%s", t->name);
    }

    util_display_render(&display);
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int taskfd;
    void (*formats[])(void) = {
        [PS_NORMAL] = print_list_format,
        [PS_LIST] = print_list_format,
        [PS_SIGNAL] = print_signal_format,
    };

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_lng:
            display_choice = PS_LIST;
            break;

        case ARG_signals:
            display_choice = PS_SIGNAL;
            break;

        case ARG_EXTRA:

        case ARG_ERR:
        default:
            return 0;
        }
    }

    taskfd = open(TASK_API_FILE, O_RDONLY);

    if (taskfd < 0) {
        perror(TASK_API_FILE);
        return 0;
    }

    while (task_count < TASK_MAX
           && read(taskfd, tinfo + task_count, sizeof(*tinfo)) != 0)
        task_count++;

    close(taskfd);

    (formats[display_choice])();

    return 0;
}

