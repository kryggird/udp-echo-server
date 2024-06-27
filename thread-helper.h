#define _GNU_SOURCE

#include <pthread.h> // pthread_create, pthread_join
#include <sched.h> // sched_setaffinity, CPU_SETSIZE, CPU_ZERO, CPU_SET
#include <stddef.h> // NULL
#include <stdio.h> // perror
#include <unistd.h>

#include "server-helper.h" // run_server

#define SYS_NCPUS 1024

typedef struct {
    int thread_id;
    struct parameters* params;
} thread_args;

void* run_one(void* arg) {
    thread_args* args = (thread_args*) arg;
    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(args->thread_id % SYS_NCPUS, &cpu_set);
    // `sched_setaffinity`: If pid is zero, then the calling thread is used.  
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set)!= 0) {
        perror("sched_setaffinity");
        return NULL;
    }

    run_server(/*args->params*/);
    return NULL;
}

void run_many(struct parameters* params) {
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[num_cpus];
    thread_args args[num_cpus];

    for (int i = 0; i < num_cpus; i++) {
        args[i] = (thread_args){
         .thread_id = i,
         .params = params
        };
        if (pthread_create(&threads[i], NULL, run_one, &args[i])!= 0) {
            perror("pthread_create");
            return;
        }
    }

    for (int i = 0; i < num_cpus; i++) {
        pthread_join(threads[i], NULL);
    }
}
