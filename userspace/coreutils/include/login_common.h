#ifndef _LOGIN_COMMON_H
#define _LOGIN_COMMON_H

#include "db_passwd.h"
#include "db_group.h"

void swap_user(struct passwd_entry *user, struct group_db *group_db);
char *read_password(const char *prompt);

#endif
