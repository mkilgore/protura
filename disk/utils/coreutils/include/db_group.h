#ifndef _DB_GROUP_H
#define _DB_GROUP_H

#include <stdio.h>
#include "list.h"

struct group_member {
    list_node_t entry;
    char *username;
};

struct group {
    list_node_t entry;

    char *group_name;
    char *password;
    gid_t gid;
    list_head_t member_list;
};

struct group_db {
    list_head_t group_list;
};

#define GROUP_MEMBER_INIT(g) \
    { \
        .entry = LIST_NODE_INIT((g).entry), \
    }

#define GROUP_INIT(g) \
    { \
        .entry = LIST_NODE_INIT((g).entry), \
        .member_list = LIST_HEAD_INIT((g).member_list), \
    }

#define GROUP_DB_INIT(g) \
    { \
        .group_list = LIST_HEAD_INIT((g).group_list), \
    }

static inline void group_member_init(struct group_member *g)
{
    *g = (struct group_member)GROUP_MEMBER_INIT(*g);
}

static inline void group_init(struct group *g)
{
    *g = (struct group)GROUP_INIT(*g);
}

static inline void group_db_init(struct group_db *g)
{
    *g = (struct group_db)GROUP_DB_INIT(*g);
}

int group_db_load(struct group_db *);
int group_db_save(const struct group_db *);

void group_member_clear(struct group_member *);
void group_clear(struct group *);
void group_db_clear(struct group_db *);

struct group *group_db_get_group(struct group_db *, const char *);
struct group *group_db_get_gid(struct group_db *, gid_t);

#endif
