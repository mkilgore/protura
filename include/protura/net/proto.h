#ifndef INCLUDE_PROTURA_NET_PROTO_H
#define INCLUDE_PROTURA_NET_PROTO_H

#include <protura/fs/inode.h>
#include <protura/fs/seq_file.h>
#include <protura/net/sockaddr.h>
#include <protura/mm/user_check.h>
#include <protura/mutex.h>
#include <protura/list.h>

struct packet;
struct socket;

struct protocol {
    const char *name;
    struct protocol_ops *ops;

    mutex_t lock;
    list_head_t socket_list;
};

#define PROTOCOL_INIT(nam, pro, op) \
    { \
        .name = (nam), \
        .ops = (op), \
        .socket_list = LIST_HEAD_INIT((pro).socket_list), \
        .lock = MUTEX_INIT((pro).lock, "proto-socket-list-lock"), \
    }

static inline void protocol_init(struct protocol *proto, const char *name, struct protocol_ops *ops)
{
    *proto = (struct protocol)PROTOCOL_INIT(name, (*proto), ops);
}

struct protocol_ops {
    void (*packet_rx) (struct protocol *, struct socket *, struct packet *);

    int (*create) (struct protocol *, struct socket *);
    void (*release) (struct protocol *, struct socket *);

    int (*shutdown) (struct protocol *, struct socket *, int how);

    int (*sendto) (struct protocol *, struct socket *, struct user_buffer buf, size_t len, const struct sockaddr *, socklen_t);

    int (*bind) (struct protocol *, struct socket *, const struct sockaddr *, socklen_t);
    int (*autobind) (struct protocol *, struct socket *);
    int (*getsockname) (struct protocol *, struct socket *, struct sockaddr *, socklen_t *);

    int (*connect) (struct protocol *, struct socket *, const struct sockaddr *, socklen_t);
    int (*accept) (struct protocol *, struct socket *, struct socket **, struct sockaddr *, socklen_t *);
};

int proto_seq_start(struct seq_file *seq, struct protocol *proto);
struct socket *proto_seq_get_socket(struct seq_file *seq);
int proto_seq_next(struct seq_file *seq);
void proto_seq_end(struct seq_file *seq);

#endif
