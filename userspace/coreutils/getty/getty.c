// getty - Open a new session and tty
#define UTILITY_NAME "getty"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <fcntl.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags]";
static const char *usage_str = "getty starts a new terminal session, and optionally opens the supplied TTY device.\n";
static const char *arg_desc_str  = "TTY device: The TTY device file to open.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(tty_device, "tty-device", 't', 1, "device", "The TTY device to open") \
    X(login, "login", 'l', 1, "executable", "The login executable to run") \
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

const char *login = "/bin/login";
const char *tty_dev = NULL;
const char *issue_file = "/etc/issue";

static const char *weekday_strs[] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
};

static const char *month_strs[] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
};

static void display_issue(FILE *f)
{
    struct utsname utsname;
    struct tm cur_time;
    time_t tim;

    time(&tim);
    localtime_r(&tim, &cur_time);
    uname(&utsname);

    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c != '\\') {
            putchar(c);
            continue;
        }

        switch (fgetc(f)) {
        case 'd':
            printf("%s %s %d  %d",
                    weekday_strs[cur_time.tm_wday],
                    month_strs[cur_time.tm_mon],
                    cur_time.tm_mday,
                    cur_time.tm_year + 1900);
            break;

        case 't':
            printf("%02d:%02d:%02d", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec);
            break;

        case 'l':
            printf("%s", ttyname(STDIN_FILENO));
            break;

        case 's':
            printf("%s", utsname.sysname);
            break;

        case 'n':
            printf("%s", utsname.nodename);
            break;

        case 'r':
            printf("%s", utsname.release);
            break;

        case 'v':
            printf("%s", utsname.version);
            break;

        case 'm':
            printf("%s", utsname.machine);
            break;

        case '\\':
            putchar('\\');
            break;
        }
    }
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int fd;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_tty_device:
            tty_dev = argarg;
            break;

        case ARG_login:
            login = argarg;
            break;

        case ARG_EXTRA:
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (tty_dev) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    setsid();

    if (tty_dev) {
        fd = open(tty_dev, O_RDWR);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }

    FILE *issue = fopen(issue_file, "r");
    if (issue) {
        display_issue(issue);
        fclose(issue);
    }

    execl(login, login, NULL);

    return 0;
}

