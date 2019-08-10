#ifndef _DB_PASSWD_H
#define _DB_PASSWD_H

#include <stdio.h>
#include "list.h"

struct passwd_entry {
    list_node_t entry;

    char *username;
    char *password;
    uid_t uid;
    gid_t gid;
    char *description;
    char *home_dir;
    char *shell;
};

struct passwd_db {
    list_head_t pw_list;
};

#define PASSWD_ENTRY_INIT(p) \
    { \
        .entry = LIST_NODE_INIT((p).entry), \
    }

#define PASSWD_DB_INIT(p) \
    { \
        .pw_list = LIST_HEAD_INIT((p).pw_list), \
    }

static inline void passwd_entry_init(struct passwd_entry *e)
{
    *e = (struct passwd_entry)PASSWD_ENTRY_INIT(*e);
}

static inline void passwd_db_init(struct passwd_db *d)
{
    *d = (struct passwd_db)PASSWD_DB_INIT(*d);
}

int passwd_db_load(struct passwd_db *);
int passwd_db_save(const struct passwd_db *);

void passwd_entry_clear(struct passwd_entry *ent);
void passwd_db_clear(struct passwd_db *pw);

#endif
