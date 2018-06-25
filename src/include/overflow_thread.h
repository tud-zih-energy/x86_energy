/*
 * overflow_thread.h
 *
 *  Created on: 22.06.2018
 *      Author: rschoene
 */

#ifndef SRC_INCLUDE_OVERFLOW_THREAD_H_
#define SRC_INCLUDE_OVERFLOW_THREAD_H_

#include <pthread.h>

#include "../../include/x86_energy.h"

typedef double ( *read_function_t )( x86_energy_single_counter_t );

struct thread_info
{
    int cpu;
    pthread_t thread;
    pthread_mutex_t mutex;
    long long usleep_time;
    size_t nr_functions;
    read_function_t * functions;
    x86_energy_single_counter_t * t;
};

struct ov_struct
{
    size_t nr_thread_infos;
    struct thread_info ** thread_infos;
};


/**
 * Sets up a new overflow thread (if necessary)
 * If not necessary, registers the read/single_counter pair
 * Returns 1 on fail
 * sets thread and mutex
 */
int x86_energy_overflow_thread_create(struct ov_struct *, int cpu, pthread_t *thread, pthread_mutex_t *mutex,
        double ( *read )( x86_energy_single_counter_t t ),
        x86_energy_single_counter_t t, long long sleep_time );

void x86_energy_overflow_thread_remove_call(struct ov_struct * ov, int cpu, double ( *read )( x86_energy_single_counter_t ),
        x86_energy_single_counter_t t);

int x86_energy_overflow_thread_killall( struct ov_struct * );
void x86_energy_overflow_freeall(struct ov_struct * ov);

#endif /* SRC_INCLUDE_OVERFLOW_THREAD_H_ */
