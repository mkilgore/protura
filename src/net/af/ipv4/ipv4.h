#ifndef SRC_NET_AF_IPV4_IPV4_H
#define SRC_NET_AF_IPV4_IPV4_H

#include <protura/fs/procfs.h>

extern struct procfs_entry_ops ipv4_route_ops;

extern struct procfs_dir *ipv4_dir_procfs;

#endif
