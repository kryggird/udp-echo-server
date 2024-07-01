#pragma once

#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>   // printf
#include <string.h>  // strerror
#include <sys/socket.h>
#include <sys/stat.h>

#include "liburing.h"
#include "ring_helper.h"
#include "socket_helper.h"
#include "stats_helper.h"

const size_t BUFFER_SIZE = 65535;
const size_t NUM_BUFFERS = 1024;

// Use a define to avoid creating VLAs
#define IO_QUEUE_DEPTH 64
#define NUM_CQE_SLOTS 1024 // IO_QUEUE_DEPTH * 16

void run_server(bool is_ip_v4, uint32_t port, bool send_zc, atomic_stats_t* stats) {
    int fd = init_socket(is_ip_v4, port);

    buffer_pool_t pool = init_buffer_pool(BUFFER_SIZE, NUM_BUFFERS);
    struct io_uring_cqe* cqe_slots[NUM_CQE_SLOTS];
    sendmsg_metadata_t sendmsg_slots[NUM_CQE_SLOTS];

    struct io_uring ring;
    int ret = init_ring(&ring, IO_QUEUE_DEPTH, NUM_CQE_SLOTS);
    if (ret != 0) {
        goto socket_cleanup;
    }
    ret = register_buffer_pool(&ring, &pool);
    if ((ret != 0) || pool.metadata == NULL) {  // Allocation failure
        goto ring_cleanup;
    }
    ret = io_uring_register_files(&ring, &fd, 1);
    int fd_index = 0; // We register only one socket!
    if (ret) {
        fprintf(stderr, "error registering buffers: %s", strerror(-ret));
        goto pool_cleanup;
    }

    struct msghdr msg = (struct msghdr){
        .msg_namelen = sizeof(struct sockaddr_storage),
        .msg_controllen = 0};
    prep_recv_multishot(&ring, &msg, fd_index);
    io_uring_submit(&ring);

    size_t recv_count = 0;
    size_t send_count = 0;
    size_t koverflow_count = 0;

    for (;;) {
        int ret = io_uring_submit_and_wait(&ring, 1);
        if (ret == -EINTR) {
            continue;
        }
        if (ret < 0) {
            break;
        }

        size_t new_cqe_count =
            io_uring_peek_batch_cqe(&ring, cqe_slots, NUM_CQE_SLOTS);

        for (size_t cqe_idx = 0; cqe_idx < new_cqe_count; ++cqe_idx) {
            op_metadata_t op_meta = {.as_u64 = cqe_slots[cqe_idx]->user_data};
            int must_rearm = !(cqe_slots[cqe_idx]->flags & IORING_CQE_F_MORE);

            if (op_meta.is_recvmsg && must_rearm) {
                prep_recv_multishot(&ring, &msg, fd_index);
                io_uring_submit(&ring);
                break;
            }
        }

        size_t buf_ring_advance = 0;

        for (size_t cqe_idx = 0; cqe_idx < new_cqe_count; ++cqe_idx) {
            op_metadata_t op_meta = {.as_u64 = cqe_slots[cqe_idx]->user_data};

            if (op_meta.is_recvmsg) {
                recvmsg_result_t res =
                    validate_recvmsg(cqe_slots[cqe_idx], &pool, &msg);
                ++recv_count;

                bool is_valid_idx = res.buffer_idx < NUM_CQE_SLOTS;
                if (res.is_valid && is_valid_idx) {
                    sendmsg_metadata_t* meta = &sendmsg_slots[res.buffer_idx];
                    prep_sendmsg(&ring, meta, &res, send_zc, fd_index);
                    ++send_count;
                } else if (is_valid_idx) {
                    add_buffer(&pool, op_meta.buffer_idx);
                    ++buf_ring_advance;
                } else {
                    fprintf(stderr, "pool index out-of-bound");
                    goto pool_cleanup;
                }

            } else {
                add_buffer(&pool, op_meta.buffer_idx);
                ++buf_ring_advance;
            }
        }

        io_uring_buf_ring_advance(pool.metadata, buf_ring_advance);

        if (stats) {
            koverflow_count += *ring.cq.koverflow;
            update_atomic_stats(stats, send_count, recv_count, koverflow_count);
        }

        io_uring_cq_advance(&ring, new_cqe_count);
    }

pool_cleanup:
    unmap_pool(&pool);
ring_cleanup:
    io_uring_queue_exit(&ring);
socket_cleanup:
    close(fd);
}
