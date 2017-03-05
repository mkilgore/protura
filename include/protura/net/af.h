#ifndef INCLUDE_PROTURA_NET_AF_H
#define INCLUDE_PROTURA_NET_AF_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/net/sockaddr.h>

struct socket;
struct packet;

struct address_family {
    list_node_t af_entry;
    int af_type;

    struct address_family_ops *ops;
};

#define ADDRESS_FAMILY_INIT(af)\
    { \
        .af_entry = LIST_NODE_INIT((af).af_entry), \
    }

static inline void address_family_init(struct address_family *af)
{
    *af = (struct address_family)ADDRESS_FAMILY_INIT(*af);
}

struct address_family_ops {
    void (*packet_rx) (struct address_family *, struct packet *);
    void (*setup_af) (struct address_family *);

    int (*create) (struct address_family *, struct socket *);
    int (*delete) (struct address_family *, struct socket *);

    int (*sendto) (struct address_family *, struct socket *, struct packet *, const struct sockaddr *, socklen_t);

    int (*bind) (struct address_family *, struct socket *, const struct sockaddr *, socklen_t);
    int (*autobind) (struct address_family *, struct socket *);
    int (*getsockname) (struct address_family *, struct socket *, struct sockaddr *, socklen_t *);
};

void address_family_register(struct address_family *);
struct address_family *address_family_lookup(int af);

void address_family_setup(void);

#endif
