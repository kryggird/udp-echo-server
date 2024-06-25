#define _GNU_SOURCE

#include <stdint.h>
// #include <fcntl.h>
// #include <unistd.h>

#include <sys/stat.h>

#include "socket_helper.h"
#include "ring_helper.h"

const size_t BUFFER_SIZE = 4096;
const size_t NUM_BUFFERS = 1024;

void run_server() {
    int fd = init_socket_v6(8080);
    
    buffer_pool_t pool = init_buffer_pool(BUFFER_SIZE, NUM_BUFFERS);

    struct io_uring ring;
    init_ring(&ring);
    register_buffer_pool(&ring, &pool);

    prep_recv_multishot(&ring, &msg);
}

int main() {
    run_server();

    return 0;
}
