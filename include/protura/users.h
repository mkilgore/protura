#ifndef INCLUDE_PROTURA_USERS_H
#define INCLUDE_PROTURA_USERS_H

#include <protura/types.h>
#include <protura/mutex.h>

#define NGROUPS 32

struct credentials {
    mutex_t cred_lock;

    uid_t uid, euid, suid;
    gid_t gid, egid, sgid;

    gid_t sup_groups[NGROUPS];
};

#define CREDENTIALS_INIT(cred) \
    { \
        .cred_lock = MUTEX_INIT((cred).cred_lock), \
        .sup_groups = { [0 ... (NGROUPS - 1)] = GID_INVALID }, \
    }

static inline void credentials_init(struct credentials *creds)
{
    *creds = (struct credentials)CREDENTIALS_INIT(*creds);
}

#define using_creds(cred) using_mutex(&(cred)->cred_lock)

int __credentials_belong_to_gid(struct credentials *, gid_t);

int sys_setuid(uid_t uid);
int sys_setreuid(uid_t ruid, uid_t euid);
int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid);

int sys_getuid(void);
int sys_geteuid(void);

int sys_setgid(gid_t gid);
int sys_setregid(gid_t rgid, gid_t egid);
int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid);

int sys_getgid(void);
int sys_getegid(void);

int sys_setgroups(size_t size, struct user_buffer list);
int sys_getgroups(size_t size, struct user_buffer list);

#endif
