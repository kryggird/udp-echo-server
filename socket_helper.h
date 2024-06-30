#pragma once

#include <stdio.h>      // perror
#include <arpa/inet.h>  // inet_pton
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY, IN6ADDR_ANY_INIT
#include <sys/socket.h> // socket, bind, AF_INET, SOCK_DGRAM, SO_REUSEPORT

// TODO Error handling
int init_socket_v6(uint32_t port) {
    uint16_t port_be = htons(port); // Handle zero?
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    //    if (fd < 0) {
    // perror("socket");
    //    }
    int opt_val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));

    struct sockaddr_in6 addr_v6 = {.sin6_family = AF_INET6,
                                   .sin6_port = port_be,
                                   .sin6_addr = IN6ADDR_ANY_INIT};
    int ret = bind(fd, (struct sockaddr *)&addr_v6, sizeof(addr_v6));

    // TODO check that port is bound

    return fd;
}

// TODO Error handling
int init_socket_v4(uint32_t port) {
    uint16_t port_be = htons(port); // Handle zero?
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    //    if (fd < 0) {
    // perror("socket");
    //    }
    int max_buf_size;
    socklen_t optlen = sizeof(max_buf_size);
    int opt_val = 1;

    int ret = 0;

    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
    if (ret < 0) {
        perror("setsockopt SO_REUSEPORT");
        return -1;
    }

    ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &max_buf_size, &optlen);
    if (ret < 0) {
        perror("getsockopt SO_RCVBUF");
        return -1;
    }

    ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &max_buf_size, optlen);
    if (ret < 0) {
        perror("setsockopt SO_RCVBUF");
        return -1;
    }

    ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &max_buf_size, &optlen);
    if (ret < 0) {
        perror("getsockopt SO_SNDBUF");
        return -1;
    }

    ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &max_buf_size, optlen);
    if (ret < 0) {
        perror("setsockopt SO_SNDBUF");
        return -1;
    }

    struct sockaddr_in addr_v4 = {
        .sin_family = AF_INET, .sin_port = port_be, .sin_addr = {INADDR_ANY}
    };

    ret = bind(fd, (struct sockaddr *)&addr_v4, sizeof(addr_v4));

    // TODO check that port is bound

    return fd;
}
