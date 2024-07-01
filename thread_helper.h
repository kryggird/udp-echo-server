#pragma once

#define _GNU_SOURCE

#include <pthread.h>  // pthread_create, pthread_join
#include <sched.h>  // sched_setaffinity, CPU_SETSIZE, CPU_ZERO, CPU_SET, sched_getaffinity
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>  // NULL
#include <stdio.h>   // perror
#include <unistd.h>

#include "server_helper.h"  // run_server
#include "stats_helper.h"

typedef struct {
    int thread_id;
    uint32_t port;
    bool ip_v4;
    bool send_zc;
    atomic_stats_t* stats;
} thread_args;

typedef struct {
    atomic_size_t cancel_token;
    atomic_stats_t* stats;
    size_t num_stats;
} stats_thread_arg;

int count_cpus(cpu_set_t* cpu_mask) {
    int num_cpus = 0;
    for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); ++i) {
        if (CPU_ISSET(i, cpu_mask)) {
            ++num_cpus;
        }
    }
    return num_cpus;
}

void* run_stats(void* arg) {
    stats_thread_arg* args = (stats_thread_arg*) arg;
    struct timespec ts = {1, 0}; // 1 second

    atomic_stats_t* per_thread_stats = args->stats;
    stats_t snapshot[args->num_stats];
    do {
        nanosleep(&ts, NULL);

        for (size_t idx = 0; idx < args->num_stats; ++idx) {
            snapshot[idx] = clone_atomic_stats(&per_thread_stats[idx]);
        }

        stats_t accum = (stats_t){ 0, 0, 0 };

        for (size_t idx = 0; idx < args->num_stats; ++idx) {
            accum = sum_stats(accum, snapshot[idx]);
        }
        printf(
            "Recv: %9zu, Sent: %9zu, Invalid: %9zu, Overflow: %9zu\n",
            accum.recv_count, accum.sent_count, (accum.recv_count - accum.sent_count),
            accum.koverflow);

    } while (atomic_load(&args->cancel_token));

    return NULL;
}

void* run_one(void* arg) {
    thread_args* args = (thread_args*)arg;
    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(args->thread_id, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) != 0) {
        perror("sched_setaffinity");
        return NULL;
    }

    printf("Running on logical core %d\n", args->thread_id);

    run_server(args->ip_v4, args->port, args->send_zc, args->stats);
    return NULL;
}

void run_many(bool ip_v4, uint32_t port, bool send_zc, bool print_stats) {
    cpu_set_t cpu_mask;
    if (sched_getaffinity(0, sizeof(cpu_mask), &cpu_mask) != 0) {
        perror("sched_getaffinity");
        return;
    }

    int max_active_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int num_active_cpus = count_cpus(&cpu_mask);

    pthread_t threads[num_active_cpus];
    thread_args args[num_active_cpus];
    atomic_stats_t stats[num_active_cpus];

    for (int idx = 0; idx < num_active_cpus; ++idx) {
        init_atomic_stats(&stats[idx]);

    }

    int thread_idx = 0;
    for (int cpu_idx = 0; cpu_idx < max_active_cpus; ++cpu_idx) {
        if (CPU_ISSET(cpu_idx, &cpu_mask)) {
            atomic_stats_t* stats_ptr = print_stats ? &stats[thread_idx] : NULL;

            args[thread_idx] = (thread_args){
                .thread_id = cpu_idx, 
                .ip_v4 = ip_v4, 
                .port = port, 
                .send_zc = send_zc, 
                .stats = stats_ptr,
            };
            if (pthread_create(&threads[thread_idx], NULL, run_one,
                               &args[thread_idx]) != 0) {
                perror("pthread_create");
                return;
            }
            ++thread_idx;
        }
    }

    pthread_t stats_thread;
    stats_thread_arg stats_arg = {
        .cancel_token = ATOMIC_VAR_INIT(1),
        .stats = stats,
        .num_stats = num_active_cpus,
    };
    if (print_stats) {
        if (pthread_create(&stats_thread, NULL, run_stats, &stats_arg)) {
            perror("stats thread");
            print_stats = false;
        }
    }

    for (int thread_idx = 0; thread_idx < num_active_cpus; ++thread_idx) {
        pthread_join(threads[thread_idx], NULL);
    }

    if (print_stats) {
        atomic_store(&stats_arg.cancel_token, 0);
        pthread_join(stats_thread, NULL);
    }
}
