#include <sys/mman.h> // mmap

#include "liburing.h"

const int IO_QUEUE_DEPTH = 64;

void init_ring(struct io_uring* ring) {
    // TODO error checking
    int ret = io_uring_queue_init_params(IO_QUEUE_DEPTH, ring, &(struct io_uring_params) {
	.cq_entries = IO_QUEUE_DEPTH,
	.flags = IORING_SETUP_SUBMIT_ALL 
	| IORING_SETUP_COOP_TASKRUN 
	| IORING_SETUP_CQSIZE    
    });
}

typedef struct {
    size_t num_buffers;
    size_t buf_size;
    size_t size;
    struct io_uring_buf_ring* metadata;
    unsigned char* buffers;
} buffer_pool_t;

buffer_pool_t init_buffer_pool(size_t buf_size, size_t num_buffers) {
    size_t mmap_size = (buf_size + sizeof(struct io_uring_buf)) * num_buffers;

    void* metadata = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    // TODO check for error
    void* buffers = metadata + sizeof(struct io_uring_buf) * num_buffers;

    return (buffer_pool_t){mmap_size, num_buffers, buf_size, metadata, buffers};
}

void register_buffer_pool(struct io_uring* ring, buffer_pool_t* pool) {
    io_uring_buf_ring_init(pool->metadata);

    struct io_uring_buf_reg reg = (struct io_uring_buf_reg) {
	.ring_addr = (unsigned long) pool->metadata,
	.ring_entries = pool->num_buffers,
	.bgid = 0
    };

    int ret = io_uring_register_buf_ring(ring, &reg, 0);
    for (size_t idx = 0; idx < pool->num_buffers; ++idx) {
	io_uring_buf_ring_add(pool->metadata, 
			      pool->buffers + pool->buf_size * idx,
			      pool->buf_size, idx,
			      io_uring_buf_ring_mask(pool->num_buffers), 
			      idx);
    }
    io_uring_buf_ring_advance(pool->metadata, pool->num_buffers);
}

int prep_recv_multishot(struct io_uring* ring, struct msghdr* msg) {
	struct io_uring_sqe* sqe = io_uring_get_sqe(ring);

	io_uring_prep_recvmsg_multishot(
	    sqe, 0 /* First registered fd. TODO: thread index */, 
	    msg, MSG_TRUNC);
	sqe->flags |= IOSQE_FIXED_FILE | IOSQE_BUFFER_SELECT;
	sqe->buf_group = 0;

	io_uring_sqe_set_data64(sqe, 0);
	return 0;
}

