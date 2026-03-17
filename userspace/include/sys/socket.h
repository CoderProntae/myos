#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stdint.h>

#define AF_INET     2
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

#define SOL_SOCKET  1
#define SO_REUSEADDR 2

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char     sin_zero[8];
};

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* addr, uint32_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, uint32_t* addrlen);
int connect(int sockfd, const struct sockaddr* addr, uint32_t addrlen);
ssize_t send(int sockfd, const void* buf, size_t len, int flags);
ssize_t recv(int sockfd, void* buf, size_t len, int flags);
int shutdown(int sockfd, int how);
int setsockopt(int sockfd, int level, int optname, const void* optval, uint32_t optlen);

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#endif
