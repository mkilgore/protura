#ifndef INCLUDE_PROTURA_NET_SYS_H
#define INCLUDE_PROTURA_NET_SYS_H

#include <protura/fs/file.h>
#include <protura/net/sockaddr.h>

int sys_socket(int domain, int type, int protocol);

int sys_send(int sockfd, const void *buf, size_t len, int flags);
int sys_recv(int sockfd, void *buf, size_t len, int flags);

int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen);
int sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

int sys_shutdown(int sockfd, int how);


int __sys_sendto(struct file *, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen);
int __sys_recvfrom(struct file *, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

#endif
