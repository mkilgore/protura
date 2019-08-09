#ifndef INCLUDE_PROTURA_USERS_H
#define INCLUDE_PROTURA_USERS_H

#include <protura/types.h>
#include <protura/mm/user_ptr.h>

int sys_setuid(uid_t uid);
int sys_seteuid(uid_t uid);
int sys_setreuid(uid_t ruid, uid_t euid);
int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid);

int sys_getuid(void);
int sys_geteuid(void);

int sys_setgid(gid_t gid);
int sys_setegid(gid_t gid);
int sys_setregid(gid_t rgid, gid_t egid);
int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid);

int sys_getgid(void);
int sys_getegid(void);

int sys_setgroups(size_t size, const gid_t *__user list);

#endif
