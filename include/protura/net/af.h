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

#define ADDRESS_FAMILY_INIT(af, type, op)\
    { \
        .af_entry = LIST_NODE_INIT((af).af_entry), \
        .af_type = type, \
        .ops = op, \
    }

static inline void address_family_init(struct address_family *af, int af_type, struct address_family_ops *ops)
{
    *af = (struct address_family)ADDRESS_FAMILY_INIT(*af, af_type, ops);
}

struct address_family_ops {
    void (*packet_rx) (struct address_family *, struct packet *);
    int (*packet_tx) (struct address_family *, struct packet *);

    void (*setup_af) (struct address_family *);

    /* Parses the sockaddr (can be NULL) or uses other information to add routing informatiog to the packet */
    int (*process_sockaddr) (struct address_family *, struct packet *, const struct sockaddr *, socklen_t len);

    int (*create) (struct address_family *, struct socket *);
    int (*delete) (struct address_family *, struct socket *);

    int (*sendto) (struct address_family *, struct socket *, struct packet *, const struct sockaddr *, socklen_t);

    int (*bind) (struct address_family *, struct socket *, const struct sockaddr *, socklen_t);
    int (*autobind) (struct address_family *, struct socket *);
    int (*getsockname) (struct address_family *, struct socket *, struct sockaddr *, socklen_t *);

    int (*connect) (struct address_family *, struct socket *, const struct sockaddr *, socklen_t);
    int (*accept) (struct address_family *, struct socket *, struct socket **, struct sockaddr *, socklen_t *);
};

void address_family_register(struct address_family *);
struct address_family *address_family_lookup(int af);

void address_family_setup(void);

#endif
