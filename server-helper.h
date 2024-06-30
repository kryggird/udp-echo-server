#pragma once

#define _GNU_SOURCE

#include <stdint.h>

#include <sys/stat.h>
#include <sys/socket.h>

#include "liburing.h"
#include "socket_helper.h"
#include "ring_helper.h"


const size_t BUFFER_SIZE = 4096;
const size_t NUM_BUFFERS = 1024;

const int IO_QUEUE_DEPTH = 64;
const int NUM_CQE_SLOTS = IO_QUEUE_DEPTH * 16;

#include <stdio.h>

void run_server(void) {
    int fd = init_socket_v6(8080);
    
    buffer_pool_t pool = init_buffer_pool(BUFFER_SIZE, NUM_BUFFERS);
    struct io_uring_cqe* cqe_slots[NUM_CQE_SLOTS];
    sendmsg_metadata_t sendmsg_slots[NUM_CQE_SLOTS];

    struct io_uring ring;
    init_ring(&ring, IO_QUEUE_DEPTH);
    register_buffer_pool(&ring, &pool);
    io_uring_register_files(&ring, &fd, 1); // TODO get registered file idx

    struct msghdr msg = (struct msghdr){
	.msg_namelen = sizeof(struct sockaddr_storage), // TODO Why not 0?
	.msg_controllen = 0
    };
    prep_recv_multishot(&ring, &msg);
    io_uring_submit(&ring);

    size_t recv_count = 0;
    size_t send_count = 0;

    for (;;) {
	int ret = io_uring_submit_and_wait(&ring, 1);
	if (ret == -EINTR) { continue; }
	if (ret < 0) { break; }

	size_t new_cqe_count = io_uring_peek_batch_cqe(&ring, cqe_slots, NUM_CQE_SLOTS);

	for (size_t cqe_idx = 0; cqe_idx < new_cqe_count; ++cqe_idx) {
	    op_metadata_t op_meta = { .as_u64 = cqe_slots[cqe_idx]->user_data };
	    int must_rearm = !(cqe_slots[cqe_idx]->flags & IORING_CQE_F_MORE);
	    if (op_meta.is_recvmsg && must_rearm) {
		prep_recv_multishot(&ring, &msg);
		io_uring_submit(&ring);
		break;
	    }
	}

	size_t old_recv_count = recv_count;
	size_t buf_ring_advance = 0;

	for (size_t cqe_idx = 0; cqe_idx < new_cqe_count; ++cqe_idx) {
	    // TODO: Handle IORING_CQE_F_MORE and IORING_CQE_F_BUFFER
	    
	    op_metadata_t op_meta = { .as_u64 = cqe_slots[cqe_idx]->user_data };
	
	    if (op_meta.is_recvmsg) {
		recvmsg_result_t res = validate_recvmsg(cqe_slots[cqe_idx], &pool, &msg);
		++recv_count;

		if (res.is_valid) {
		    prep_sendmsg(&ring, sendmsg_slots, &res);
		    ++send_count;
		} else {
		    add_buffer(&pool, op_meta.buffer_idx);
		    ++buf_ring_advance;
		}

	    } else {
		add_buffer(&pool, op_meta.buffer_idx);
		++buf_ring_advance;
	    }
	}
	
	io_uring_buf_ring_advance(pool.metadata, buf_ring_advance);

	if ((recv_count / (32 * 1024)) > (old_recv_count / (32 * 1024))) {
	    printf("Recv (k): %5zu, Sent (k): %5zu, Invalid: %7zu, Overflow: %5u\n", 
		    recv_count / 1000, send_count / 1000, (recv_count - send_count), *ring.cq.koverflow);
	}
	
	io_uring_cq_advance(&ring, new_cqe_count);
    }

    // Cleanup
    io_uring_queue_exit(&ring);
    close(fd);
}
