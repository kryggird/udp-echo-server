#define _GNU_SOURCE

#include <pthread.h> // pthread_create, pthread_join
#include <sched.h> // sched_setaffinity, CPU_SETSIZE, CPU_ZERO, CPU_SET, sched_getaffinity
#include <stddef.h> // NULL
#include <stdio.h> // perror
#include <unistd.h>
#include "server-helper.h" // run_server

typedef struct {
    int thread_id;
    struct parameters* params;
} thread_args;

int count_cpus(cpu_set_t* cpu_mask) {
    int num_cpus = 0;
    for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); i++) {
        if (CPU_ISSET(i, cpu_mask)) {
            num_cpus++;
        }
    }
    return num_cpus;
}

void* run_one(void* arg) {
    thread_args* args = (thread_args*) arg;
    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(args->thread_id, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set)!= 0) {
        perror("sched_setaffinity");
        return NULL;
    }

    run_server(/*args->params*/);
    return NULL;
}

void run_many(struct parameters* params) {
    cpu_set_t cpu_mask;
    int num_active_cpus;

    if (sched_getaffinity(0, sizeof(cpu_mask), &cpu_mask)!= 0) {
        perror("sched_getaffinity");
        return;
    }

    num_active_cpus = count_cpus(&cpu_mask);

    pthread_t threads[num_active_cpus];
    thread_args args[num_active_cpus];
    int thread_idx = 0;

    for (int cpu_idx = 0; cpu_idx < sysconf(_SC_NPROCESSORS_ONLN); cpu_idx++) {
        if (CPU_ISSET(cpu_idx, &cpu_mask)) {
            args[thread_idx] = (thread_args){
          .thread_id = cpu_idx,
          .params = params
            };
            if (pthread_create(&threads[thread_idx], NULL, run_one, &args[thread_idx])!= 0) {
                perror("pthread_create");
                return;
            }
            thread_idx++;
        }
    }

    for (int thread_idx = 0; thread_idx < num_active_cpus; thread_idx++) {
        pthread_join(threads[thread_idx], NULL);
    }
}

