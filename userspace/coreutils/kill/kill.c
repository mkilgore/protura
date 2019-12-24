// kill - Send a signal to a process
#define UTILITY_NAME "kill"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [pids]";
static const char *usage_str = "Send the signal specified in Flags to the processes with pids.\n";
static const char *arg_desc_str  = "pids: The Process ID's to send the signal too.\n";

const char *signames[] = {
    [SIGHUP] = "SIGHUP",
    [SIGINT] = "SIGINT",
    [SIGQUIT] = "SIGQUIT",
    [SIGILL] = "SIGILL",
    [SIGTRAP] = "SIGTRAP",
    [SIGABRT] = "SIGABRT",
    [SIGIOT] = "SIGIOT",
    [SIGBUS] = "SIGBUS",
    [SIGFPE] = "SIGFPE",
    [SIGKILL] = "SIGKILL",
    [SIGUSR1] = "SIGUSR1",
    [SIGSEGV] = "SIGSEGV",
    [SIGUSR2] = "SIGUSR2",
    [SIGPIPE] = "SIGPIPE",
    [SIGALRM] = "SIGALRM",
    [SIGTERM] = "SIGTERM",
    [SIGSTKFL] = "SIGSTKFL",
    [SIGCHLD] = "SIGCHLD",
    [SIGCONT] = "SIGCONT",
    [SIGSTOP] = "SIGSTOP",
    [SIGTSTP] = "SIGTSTP",
    [SIGTTIN] = "SIGTTIN",
    [SIGTTOU] = "SIGTTOU",
    [SIGURG] = "SIGURG",
    [SIGXCPU] = "SIGXCPU",
    [SIGXFSZ] = "SIGXFSZ",
    [SIGVTALR] = "SIGVTALR",
    [SIGPROF] = "SIGPROF",
    [SIGWINCH] = "SIGWINCH",
    [SIGIO] = "SIGIO",
};

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(signal, "signal", 's', 1, "SigID or Num", "Signal to send") \
    X(list, "list", 'l', 0, NULL, "List all possible signals") \
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


int sig_send = SIGTERM;
int pid_count = 0;
pid_t pids[100];

int get_sig_num(const char *sid)
{
    int i;

    if (strncmp(sid, "SIG", 3) == 0)
        sid += 3;

    for (i = 0; i < ARRAY_SIZE(signames); i++)
        if (*signames)
            if (strcmp(*signames + 3, sid) == 0)
                return i;

    return 0;
}

void list_signals(void)
{
    int i;
    for (i = 1; i < ARRAY_SIZE(signames); i++) {
        printf("%-2d) %-12s", i, signames[i]);
        if ((i % 5) == 0)
            putchar('\n');
    }

    putchar('\n');
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int i;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_signal:
            sig_send = atoi(argarg);
            if (!sig_send)
                sig_send = get_sig_num(argarg);
            break;

        case ARG_list:
            list_signals();
            return 0;

        case ARG_EXTRA:
            pids[pid_count] = atoi(argarg);
            pid_count++;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (sig_send == 0)
        return 0;

    for (i = 0; i < pid_count; i++) {
        printf("Sending %d to %d\n", sig_send, pids[i]);
        kill(pids[i], sig_send);
    }

    return 0;
}

