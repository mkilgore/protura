#ifndef INCLUDE_PROTURA_NET_SYS_H
#define INCLUDE_PROTURA_NET_SYS_H

#include <protura/fs/file.h>
#include <protura/net/sockaddr.h>

int sys_socket(int domain, int type, int protocol);

int sys_send(int sockfd, struct user_buffer buf, size_t len, int flags);
int sys_recv(int sockfd, struct user_buffer buf, size_t len, int flags);

int sys_sendto(int sockfd, struct user_buffer buf, size_t len, int flags, struct user_buffer dest, socklen_t addrlen);
int sys_recvfrom(int sockfd, struct user_buffer buf, size_t len, int flags, struct user_buffer src_addr, struct user_buffer addrlen);

int sys_bind(int sockfd, struct user_buffer addr, socklen_t addrlen);
int sys_getsockname(int sockfd, struct user_buffer addr, struct user_buffer addrlen);

int sys_setsockopt(int sockfd, int level, int optname, struct user_buffer optval, socklen_t optlen);
int sys_getsockopt(int sockfd, int level, int optname, struct user_buffer optval, struct user_buffer optlen);

int sys_shutdown(int sockfd, int how);

int sys_accept(int sockfd, struct user_buffer addr, struct user_buffer addrlen);
int sys_connect(int sockfd, struct user_buffer addr, socklen_t addrlen);
int sys_listen(int sockfd, int backlog);

int __sys_sendto(struct file *, struct user_buffer buf, size_t len, int flags, struct user_buffer dest, socklen_t addrlen);
int __sys_recvfrom(struct file *, struct user_buffer buf, size_t len, int flags, struct user_buffer src_addr, struct user_buffer addrlen);

#endif
