
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <protura/syscall.h>
#include <sys/mount.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

#define LOGFILE "/tmp/init.log"
#define INITTAB "/etc/inittab"

const char *prog_name;

static FILE *ilog;
static char *const env_vars[] = { "PATH=/bin:/usr/bin", "TERM=protura", NULL };

enum tab_action {
    TAB_RESPAWN,
    TAB_ONCE,
    TAB_WAIT,
};

struct tab_ent {
    pid_t pid;

    unsigned int start :1;
    unsigned int finished :1;

    int level;

    char *id;
    char **args;

    enum tab_action action;

    struct tab_ent *next;
};

struct tab_ent *tab_list;

pid_t shell[2];

static int ilogf(const char *format, ...)
{
    int ret;
    va_list lst;
    va_start(lst, format);

    ret = vfprintf(ilog, format, lst);
    fflush(ilog);

    va_end(lst);
    return ret;
}

static char **parse_args(char *prog)
{
    int count = 0;
    char *l;
    char **args;

    args = malloc(sizeof(*args) * (count + 1));
    *args = NULL;

    l = prog;

    while (*l) {
        char *s;
        while (*l && (*l == ' ' || *l == '\t'))
            l++;

        if (!*l)
            break;

        args = realloc(args, sizeof(*args) * (count + 2));
        args[count + 1] = NULL;

        s = l;

        while (*l && *l != ' ' && *l != '\t')
            l++;

        if (*l) {
            *l = '\0';
            l++;
        }

        args[count] = strdup(s);
        count++;
    }

    return args;
}

static void load_tab(const char *tab)
{
    FILE *file = fopen(tab, "r");
    char *line;
    size_t buf_len;
    ssize_t len;

    while ((len = getline(&line, &buf_len, file)) != EOF) {
        char *l = line;
        char *next_ptr = NULL;
        struct tab_ent *ent;
        char *type;

        line[len - 1] = '\0';

        ilogf("inittab: %s\n", line);

        while (*l && (*l == ' ' || *l == '\t'))
            l++;

        if (!*l)
            continue;

        if (*l == '#')
            continue;

        ent = malloc(sizeof(*ent));
        memset(ent, 0, sizeof(*ent));

        ent->pid = -1;
        ent->start = 1;

        ent->id = strdup(strtok_r(l, ":", &next_ptr));
        ent->level = strtol(strtok_r(NULL, ":", &next_ptr), NULL, 10);

        type = strtok_r(NULL, ":", &next_ptr);
        if (strcmp(type, "respawn") == 0)
            ent->action = TAB_RESPAWN;
        else if (strcmp(type, "once") == 0)
            ent->action = TAB_ONCE;
        else
            ent->action = TAB_WAIT;

        ilogf("inittab: id: %s, level: %d, action: %d\n", ent->id, ent->level, ent->action);
        ilogf("inittab: next_ptr: %s\n", next_ptr);
        ent->args = parse_args(next_ptr);

        ent->next = tab_list;
        tab_list = ent;
    }

    fclose(file);
}

/* Reap children */
static void handle_children(int sig)
{
    struct tab_ent *ent;
    pid_t child;

    while ((child = waitpid(-1, NULL, WNOHANG)) > 0) {
        ilogf("Init: Reaped %d\n", child);

        for (ent = tab_list; ent; ent = ent->next) {
            if (ent->pid == child) {
                ent->pid = -1;
                switch (ent->action) {
                case TAB_RESPAWN:
                    ent->start = 1;
                    break;

                case TAB_ONCE:
                case TAB_WAIT:
                    ent->finished = 1;
                    break;
                }

                break;
            }
        }
    }
}

static void start_child(const char *prog, char *const argv[], char *const envp[])
{
    sigset_t set;
    char stdout_file[256];
    char stderr_file[256];

    pid_t pid = getpid();

    snprintf(stdout_file, sizeof(stdout_file), "%s.%d.out", LOGFILE, pid);
    snprintf(stderr_file, sizeof(stderr_file), "%s.%d.err", LOGFILE, pid);

    int logfd = fileno(ilog);
    close(logfd);

    int std_in = open("/dev/null", O_RDONLY);
    int std_out = open(stdout_file, O_RDWR | O_CREAT, 0666);
    int std_err = open(stderr_file, O_RDWR | O_CREAT, 0666);

    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
    execve(prog, argv, envp);
    exit(0);
}

static pid_t start_prog(const char *prog, char *const argv[], char *const envp[])
{
    pid_t child_pid;

    switch ((child_pid = fork())) {
    case -1:
        /* Fork error */
        return -1;

    case 0:
        /* In child */
        start_child(prog, argv, envp);

    default:
        /* In parent */
        return child_pid;
    }
}

int main(int argc, char **argv)
{
    int ret;
    struct sigaction action;
    struct tab_ent *ent;
    sigset_t sigset;

    prog_name = argv[0];

    ilog = fopen(LOGFILE, "w+");
    ilogf("Init: Booting\n");

    memset(&action, 0, sizeof(action));

    action.sa_handler = handle_children;
    sigaction(SIGCHLD, &action, NULL);

    ilogf("Init: Mounting proc.\n");
    /* Mount proc if we can */
    ret = mount(NULL, "/proc", "proc", 0, NULL);
    if (ret)
        ilogf("Init: Error mounting proc: %s\n", strerror(errno));

    setpgid(0, 0);

    ilogf("Init: Loading %s\n", INITTAB);

    load_tab(INITTAB);

    sigfillset(&sigset);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    sigemptyset(&sigset);

    for (ent = tab_list; ent; ent = ent->next) {
        if (ent->action != TAB_WAIT)
            continue;

        ent->pid = start_prog(ent->args[0], ent->args, env_vars);
        ent->start = 0;

        ilogf("Init: Running %s\n", ent->args[0]);

        while (!ent->finished)
            sigsuspend(&sigset);
    }

    while (1) {
        ilogf("Init: Checking for processes to restart\n");
        for (ent = tab_list; ent; ent = ent->next) {
            if (ent->start) {
                ent->start = 0;
                ilogf("Init: Starting %s\n", ent->args[0]);
                ent->pid = start_prog(ent->args[0], ent->args, env_vars);
            }
        }
        sigsuspend(&sigset);
    }

    return 0;
}

