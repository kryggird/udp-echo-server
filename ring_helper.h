#pragma once

#include <sys/mman.h> // mmap

#include "liburing.h"

void init_ring(struct io_uring* ring, unsigned int queue_depth) {
    // TODO error checking
    int ret = io_uring_queue_init_params(queue_depth, ring, &(struct io_uring_params) {
	.cq_entries = queue_depth * 8u,
	.flags = IORING_SETUP_SUBMIT_ALL 
	| IORING_SETUP_COOP_TASKRUN 
	| IORING_SETUP_CQSIZE    
    });
}

typedef union op_metadata_t {
    uint64_t as_u64;
    struct {
	uint32_t is_recvmsg;
	uint32_t buffer_idx;
    };
} op_metadata_t;

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

    return (buffer_pool_t){num_buffers, buf_size, mmap_size, metadata, buffers};
}

unsigned char* buffer_pointer(buffer_pool_t* pool, size_t idx) {
    return pool->buffers + idx * pool->buf_size;
}

void add_buffer(buffer_pool_t* pool, size_t idx) {
    io_uring_buf_ring_add(pool->metadata, 
			  buffer_pointer(pool, idx),
			  pool->buf_size, idx,
			  io_uring_buf_ring_mask(pool->num_buffers), 
			  idx);
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
	// io_uring_buf_ring_add(pool->metadata, 
	// 		      buffer_pointer(pool, idx),
	// 		      pool->buf_size, idx,
	// 		      io_uring_buf_ring_mask(pool->num_buffers), 
	// 		      idx);
	add_buffer(pool, idx);
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

	op_metadata_t meta = {.buffer_idx = 0, .is_recvmsg = 1};
	io_uring_sqe_set_data64(sqe, meta.as_u64);
	return 0;
}

typedef struct recvmsg_result_t {
    int is_valid;
    size_t buffer_idx;
    void* msg_name;
    uint32_t msg_name_length;
    void* msg_payload;
    unsigned int msg_payload_length;
} recvmsg_result_t;

recvmsg_result_t validate_recvmsg(struct io_uring_cqe* cqe, buffer_pool_t* pool, struct msghdr* msg) {
    size_t idx = ((size_t) cqe->flags) >> 16;

    struct io_uring_recvmsg_out* slot = io_uring_recvmsg_validate(buffer_pointer(pool, idx), cqe->res, msg);

    if (!slot || slot->namelen > msg->msg_namelen || slot->flags & MSG_TRUNC) {
	return (recvmsg_result_t){0};
    }

    return (recvmsg_result_t){
	.is_valid = 1, 
	.buffer_idx = idx,
	.msg_name = io_uring_recvmsg_name(slot),
	.msg_name_length = slot->namelen,
	.msg_payload = io_uring_recvmsg_payload(slot, msg),
	.msg_payload_length = io_uring_recvmsg_payload_length(slot, cqe->res, msg)
    };
}

typedef struct sendmsg_metadata_t {
    struct iovec iov;
    struct msghdr msghdr;
} sendmsg_metadata_t;

struct io_uring_sqe* maybe_submit_and_get_sqe(struct io_uring* ring)  {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    
    if (!sqe) {
	// Retry after clearing the submission ring
	io_uring_submit(ring);
	sqe = io_uring_get_sqe(ring);
    }

    return sqe;
}

// TODO bounds check
int prep_sendmsg(struct io_uring* ring, sendmsg_metadata_t metadata_array[], recvmsg_result_t* res) {
    // TODO: flush submit ring if one cannot get an SQE?
    struct io_uring_sqe* sqe = maybe_submit_and_get_sqe(ring);

    sendmsg_metadata_t* meta = &metadata_array[res->buffer_idx];
    meta->iov = (struct iovec) {
	.iov_base = res->msg_payload,
	.iov_len = res->msg_payload_length,
    };
    meta->msghdr = (struct msghdr) {
	.msg_namelen = res->msg_name_length,
	.msg_name = res->msg_name,
	.msg_control = NULL,
	.msg_controllen = 0,
	.msg_iov = &(meta->iov),
	.msg_iovlen = 1
    };

    op_metadata_t op_meta = {.buffer_idx = (uint32_t) res->buffer_idx, .is_recvmsg = 0};

    io_uring_prep_sendmsg_zc(sqe, 0 /* registered socket idx */, &(meta->msghdr), 0);
    io_uring_sqe_set_data64(sqe, op_meta.as_u64);
    sqe->flags |= IOSQE_FIXED_FILE; // TODO fold into flags argument of prep_sendmsg?

    return 0;
}

