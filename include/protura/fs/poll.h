#ifndef INCLUDE_PROTURA_FS_POLL_H
#define INCLUDE_PROTURA_FS_POLL_H

#include <uapi/protura/fs/poll.h>
#include <protura/types.h>
#include <protura/wait.h>

struct poll_table;

void poll_table_add(struct poll_table *, struct wait_queue *);

int sys_poll(struct user_buffer pollfds, nfds_t nfds, int timeout);

#endif
