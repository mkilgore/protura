#ifndef INCLUDE_PROTURA_NET_SOCKET_H
#define INCLUDE_PROTURA_NET_SOCKET_H

#include <protura/fs/inode.h>
#include <protura/hlist.h>
#include <protura/net/sockaddr.h>
#include <protura/net/af/ipv4.h>
#include <protura/net/proto/udp.h>

struct packet;
struct address_family;
struct protocol;

enum socket_flags {
    SOCKET_IS_BOUND,
    SOCKET_IS_CLOSED,
};

struct socket {
    atomic_t refs;

    flags_t flags;

    int address_family;
    int sock_type;
    int protocol;

    list_node_t socket_entry;

    hlist_node_t socket_hash_entry;

    struct address_family *af;
    struct protocol *proto;

    union {
        struct ipv4_socket_private ipv4;
    } af_private;

    union {
        struct udp_socket_private udp;
    } proto_private;

    mutex_t recv_lock;
    struct wait_queue recv_wait_queue;
    list_head_t recv_queue;
};

#define SOCKET_INIT(sock) \
    { \
        .refs = ATOMIC_INIT(0), \
        .socket_entry = LIST_NODE_INIT((sock).socket_entry), \
        .socket_hash_entry = HLIST_NODE_INIT(), \
        .recv_lock = MUTEX_INIT((sock).recv_lock, "socket-recv-lock"), \
        .recv_wait_queue = WAIT_QUEUE_INIT((sock).recv_wait_queue, "socket-recv-wait-queue"), \
        .recv_queue = LIST_HEAD_INIT((sock).recv_queue), \
    }

static inline void socket_init(struct socket *socket)
{
    *socket = (struct socket)SOCKET_INIT(*socket);
}

struct socket *socket_alloc(void);
void socket_free(struct socket *socket);

static inline struct socket *socket_dup(struct socket *socket)
{
    atomic_inc(&socket->refs);
    return socket;
}

static inline void socket_put(struct socket *socket)
{
    if (atomic_dec_and_test(&socket->refs))
        socket_free(socket);
}

void socket_subsystem_init(void);

extern struct procfs_entry_ops socket_procfs;

#endif
