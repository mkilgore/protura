/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/sysmacros.h>
#include <fcntl.h>

static pid_t start_test(char *test)
{
    pid_t child_pid;
    char *sh = "/bin/sh";

    char *const argv[] = {
        sh,
        test,
        NULL
    };

    switch ((child_pid = fork())) {
    case -1:
        return -1;

    case 0: /* child */
        return execv(sh, argv);

    default:
        return child_pid;
    }
}

int main(int argc, char **argv)
{
    char *test = argv[1];

    if (!test)
        return 1;

    /* We need to open files for STDIN, STDOUT, and STDERR */
    __unused int sin = open("/dev/null", O_RDONLY);
    __unused int sout = open("/dev/null", O_RDWR);
    __unused int serr = open("/dev/null", O_RDWR);

    /* We're not running devd, so manually create the few dev nodes we need */
    mknod("/dev/hdb", S_IFBLK | 0660, makedev(8, 256));
    mknod("/dev/qemudbg", S_IFCHR | 0666, makedev(8, 0));

    setenv("PATH", "/bin:/usr/bin", 1);
    mount(NULL, "/proc", "proc", 0, NULL);
    mount("/dev/hdb", "/mnt/slave", "ext2", 0, NULL);

    printf("TEST: %s\n", test);
    pid_t test_pid = start_test(test);

    printf("TEST pid: %d\n", test_pid);

    waitpid(test_pid, NULL, 0);

    sync();
    reboot(PROTURA_REBOOT_MAGIC1, PROTURA_REBOOT_MAGIC2, PROTURA_REBOOT_RESTART);
}
