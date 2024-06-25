#include <sys/socket.h> // socket, bind, AF_INET, SOCK_DGRAM
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY, IN6ADDR_ANY_INIT
#include <arpa/inet.h> // inet_pton

// TODO Error handling
int init_socket_v6(uint32_t port) {
    uint16_t port_be = htons(port); // Handle zero?
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);

    struct sockaddr_in6 addr_v6 = {
	    .sin6_family = AF_INET6,
	    .sin6_port = port_be,
	    .sin6_addr = IN6ADDR_ANY_INIT
    };
    int ret = bind(fd, (struct sockaddr *) &addr_v6, sizeof(addr_v6));
    
    // TODO check that port is bound

    return fd;
}

// TODO Error handling
int init_socket_v4(uint32_t port) {
    uint16_t port_be = htons(port); // Handle zero?
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr_v4 = {
			.sin_family = AF_INET,
			.sin_port = port_be,
			.sin_addr = { INADDR_ANY }
		};

    int ret = bind(fd, (struct sockaddr *) &addr_v4, sizeof(addr_v4));

    // TODO check that port is bound

    return fd;
}
