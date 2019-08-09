/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/snprintf.h>
#include <protura/scheduler.h>

int sys_setuid(uid_t uid)
{
    struct task *current = cpu_get_local()->current;

    current->uid = current->euid = current->suid = uid;

    return 0;
}

int sys_seteuid(uid_t uid)
{
    struct task *current = cpu_get_local()->current;
    current->euid = uid;
    return 0;
}

int sys_setreuid(uid_t ruid, uid_t euid)
{

}

int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{

}

int sys_getuid(void)
{

}

int sys_geteuid(void)
{

}

int sys_setgid(gid_t gid)
{

}

int sys_setegid(gid_t gid)
{

}

int sys_setregid(gid_t rgid, gid_t egid)
{

}

int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{

}

int sys_getgid(void)
{

}

int sys_getegid(void)
{

}

int sys_setgroups(size_t size, const gid_t *__user list)
{

}
