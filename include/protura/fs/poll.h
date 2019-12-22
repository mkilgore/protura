#ifndef INCLUDE_PROTURA_FS_POLL_H
#define INCLUDE_PROTURA_FS_POLL_H

#ifdef __KERNEL__
# include <protura/types.h>
# include <protura/list.h>
# include <protura/wait.h>
#endif

#define POLLIN   0x0001
#define POLLPRI  0x0002
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020

struct pollfd {
    int fd;
    short events;
    short revents;
};

typedef unsigned int nfds_t;

#ifdef __KERNEL__

struct poll_table;

void poll_table_add(struct poll_table *, struct wait_queue *);

int sys_poll(struct user_buffer pollfds, nfds_t nfds, int timeout);

#endif

#endif
