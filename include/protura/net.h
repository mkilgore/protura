#ifndef INCLUDE_PROTURA_NET_H
#define INCLUDE_PROTURA_NET_H

#include <protura/types.h>
#include <protura/initcall.h>
#include <protura/net/types.h>
#include <protura/net/sockaddr.h>
#include <protura/net/socket.h>
#include <protura/net/if.h>
#include <protura/net/netdevice.h>
#include <protura/net/route.h>
#include <protura/net/packet.h>
#include <protura/net/af.h>
#include <protura/net/proto.h>

extern_initcall(net_procfs);
extern struct procfs_dir *net_dir_procfs;

#endif
