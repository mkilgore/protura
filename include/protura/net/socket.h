#ifndef INCLUDE_PROTURA_NET_SOCKET_H
#define INCLUDE_PROTURA_NET_SOCKET_H

#include <protura/fs/inode.h>
#include <protura/hlist.h>
#include <protura/net/sockaddr.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/ipv4/udp.h>
#include <protura/net/ipv4/tcp.h>

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
        struct tcp_socket_private tcp;
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

int socket_open(int domain, int type, int protocol, struct socket **sock_ret);

int socket_send(struct socket *, const void *buf, size_t len, int flags);
int socket_recv(struct socket *, void *buf, size_t len, int flags);

int socket_sendto(struct socket *, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen, int nonblock);
int socket_recvfrom(struct socket *, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int nonblock);

int socket_bind(struct socket *, const struct sockaddr *addr, socklen_t addrlen);
int socket_getsockname(struct socket *, struct sockaddr *addr, socklen_t *addrlen);

int socket_setsockopt(struct socket *, int level, int optname, const void *optval, socklen_t optlen);
int socket_getsockopt(struct socket *, int level, int optname, void *optval, socklen_t *optlen);

int socket_shutdown(struct socket *, int how);

#endif
