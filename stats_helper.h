#pragma once

#include <stdatomic.h>
#include <stddef.h>

#define CACHE_LINE_SIZE 128

typedef struct {
    atomic_size_t sent_count;
    atomic_size_t recv_count;
    atomic_size_t koverflow;
    unsigned char padding[128 - 3 * sizeof(atomic_size_t)];
} atomic_stats_t;

typedef struct {
    size_t sent_count;
    size_t recv_count;
    size_t koverflow;
} stats_t;

void init_atomic_stats(atomic_stats_t* stats) {
    stats->sent_count = ATOMIC_VAR_INIT(0);
    stats->recv_count = ATOMIC_VAR_INIT(0);
    stats->koverflow = ATOMIC_VAR_INIT(0);
}

void update_atomic_stats(atomic_stats_t* stats, size_t sent_count, size_t recv_count, size_t koverflow) {
    atomic_store(&stats->sent_count, sent_count);
    atomic_store(&stats->recv_count, recv_count);
    atomic_store(&stats->koverflow, koverflow);
}

stats_t clone_atomic_stats(atomic_stats_t* stats) {
    size_t sent_count = atomic_load(&stats->sent_count);
    size_t recv_count = atomic_load(&stats->recv_count);
    size_t koverflow = atomic_load(&stats->koverflow);

    return (stats_t) {sent_count, recv_count, koverflow};
}

stats_t sum_stats(stats_t lhs, stats_t rhs) {
    return (stats_t) {
        lhs.sent_count + rhs.sent_count,
        lhs.recv_count + rhs.sent_count,
        lhs.koverflow + rhs.koverflow
    };
}
