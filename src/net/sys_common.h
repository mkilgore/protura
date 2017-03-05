#ifndef SRC_NET_SYS_COMMON_H
#define SRC_NET_SYS_COMMON_H

#include <protura/fs/inode.h>
#include <protura/net/socket.h>

struct inode_socket {
    struct inode i;
    struct socket *socket;
};

extern struct file_ops socket_file_ops;

#endif
