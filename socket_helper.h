#pragma once

#define GNU_SOURCE

#include <arpa/inet.h>   // inet_pton
#include <fcntl.h>       // O_RDONLY
#include <limits.h>
#include <netinet/in.h>  // sockaddr_in, htons, INADDR_ANY, IN6ADDR_ANY_INIT
#include <stdbool.h>     // bool
#include <stdlib.h>      // strtol
#include <stdio.h>       // perror
#include <sys/socket.h>  // socket, bind, AF_INET, SOCK_DGRAM, SO_REUSEPORT
#include <unistd.h>      // open, close

#define HANDLE_ERROR(cond, err_str) \
    if ((cond)) {                   \
        perror((err_str));          \
        return -1;                  \
    }

int read_proc(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    char buffer[1024] = {0};
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1ul);
    if (bytes_read == -1) {
        perror("read");
        close(fd);
        return -1;
    } else if (bytes_read == 1024) {
        // Number is too big
        close(fd);
        return -1;
    }

    char *endptr;
    long int value = strtol(buffer, &endptr, 10);
    if (endptr == buffer || (value > (long int) INT_MAX)) {
        fprintf(stderr, "Invalid value in %s\n", filepath);
        close(fd);
        return -1;
    }

    close(fd);
    return value;
}


int init_socket(bool is_ip_v4, uint32_t port) {
    if (port == 0) {
        return -1;  // User must provide a valid port
    }
    uint16_t port_be = htons(port);  // Handle zero?

    int fd = socket(is_ip_v4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    HANDLE_ERROR(fd < 0, "socket");

    struct sockaddr* addr;
    socklen_t addr_len;
    
    struct sockaddr_in addr_v4;
    struct sockaddr_in6 addr_v6;
    if (is_ip_v4) {
        addr_v4 = (struct sockaddr_in) {
            .sin_family = AF_INET,
            .sin_port = port_be,
            .sin_addr = {INADDR_ANY}
        };
        addr = (struct sockaddr*)&addr_v4;
        addr_len = sizeof(addr_v4);
    } else {
        addr_v6 = (struct sockaddr_in6) {
            .sin6_family = AF_INET6,
            .sin6_port = port_be,
            .sin6_addr = IN6ADDR_ANY_INIT
        };
        addr = (struct sockaddr*)&addr_v6;
        addr_len = sizeof(addr_v6);
    }

    int opt_val = 1;
    int ret =
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
    HANDLE_ERROR(ret < 0, "setsockopt SO_REUSEPORT");

    int max_buf_size;
    socklen_t optlen = sizeof(max_buf_size);

    max_buf_size = read_proc("/proc/sys/net/core/rmem_max");
    if (max_buf_size > 0) {
        ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &max_buf_size, optlen);
        HANDLE_ERROR(ret < 0, "setsockopt SO_RCVBUF");
    } else {
        fprintf(stderr, "Couldn't increase rcvbuf\n");
    }

    max_buf_size = read_proc("/proc/sys/net/core/wmem_max");
    if (max_buf_size > 0) {
        ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &max_buf_size, optlen);
        HANDLE_ERROR(ret < 0, "setsockopt SO_SNDBUF");
    } else {
        fprintf(stderr, "Couldn't increase sndbuf\n");
    }

    ret = bind(fd, addr, addr_len);
    HANDLE_ERROR(ret < 0, "bind");

    return fd;
}
