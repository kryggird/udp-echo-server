#pragma once

#include <stdio.h>      // perror
#include <stdbool.h>    // bool

#include <arpa/inet.h>  // inet_pton
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY, IN6ADDR_ANY_INIT
#include <sys/socket.h> // socket, bind, AF_INET, SOCK_DGRAM, SO_REUSEPORT

#define HANDLE_ERROR(cond, err_str) \
    if ((cond)) { perror((err_str)); return -1; }

int init_socket(bool is_ip_v4, uint32_t port) {
    if (port == 0) {
        return -1; // User must provide a valid port
    }
    uint16_t port_be = htons(port); // Handle zero?

    int fd = socket(is_ip_v4? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    HANDLE_ERROR(fd < 0, "socket");

    struct sockaddr *addr;
    socklen_t addr_len;

    if (is_ip_v4) {
        struct sockaddr_in addr_v4 = {
            .sin_family = AF_INET,
            .sin_port = port_be,
            .sin_addr = {INADDR_ANY}
        };
        addr = (struct sockaddr *)&addr_v4;
        addr_len = sizeof(addr_v4);
    } else {
        struct sockaddr_in6 addr_v6 = {
         .sin6_family = AF_INET6,
         .sin6_port = port_be,
         .sin6_addr = IN6ADDR_ANY_INIT
        };
        addr = (struct sockaddr *)&addr_v6;
        addr_len = sizeof(addr_v6);
    }

    int opt_val = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
    HANDLE_ERROR(ret < 0, "setsockopt SO_REUSEPORT");

    int max_buf_size;
    socklen_t optlen = sizeof(max_buf_size);

    ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &max_buf_size, &optlen);
    HANDLE_ERROR(ret < 0, "getsockopt SO_RCVBUF");
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &max_buf_size, optlen);
    HANDLE_ERROR(ret < 0, "setsockopt SO_RCVBUF");

    ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &max_buf_size, &optlen);
    HANDLE_ERROR(ret < 0, "getsockopt SO_SNDBUF");
    ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &max_buf_size, optlen);
    HANDLE_ERROR(ret < 0, "setsockopt SO_SNDBUF");

    ret = bind(fd, addr, addr_len);
    HANDLE_ERROR(ret < 0, "bind");

    return fd;
}
