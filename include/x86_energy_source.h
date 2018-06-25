/*
 * x86_energy_source.h
 *
 *  Created on: 22.06.2018
 *      Author: rschoene
 */

#ifndef INCLUDE_X86_ENERGY_SOURCE_H_
#define INCLUDE_X86_ENERGY_SOURCE_H_

#include <stddef.h>

/**
 * Enum for different types of energy counters
 */
enum x86_energy_counter
{
    X86_ENERGY_COUNTER_PCKG = 0, /* one package / socket */
    X86_ENERGY_COUNTER_CORES = 1, /* all cores of a package */
    X86_ENERGY_COUNTER_DRAM = 2, /* dram of one package / socket */
    X86_ENERGY_COUNTER_GPU = 3, /* gpu of one package / socket */
    X86_ENERGY_COUNTER_PLATFORM = 4, /* whole platform */

    /* Non ABI */
    X86_ENERGY_COUNTER_SIZE
};

/**
 * Will be used by access sources
 */
typedef void * x86_energy_single_counter_t;

/**
 * One possible access source, e.g., likwid, perf, ...
 */
typedef struct x86_energy_access_source
{
    char * name; /** < The name of the source */
    int ( *init )( void ); /** < Call this for initialization, will return != 0 on error */
    x86_energy_single_counter_t ( *setup )( enum x86_energy_counter, size_t index ); /** < Call this for adding a counter, will return NULL on error */
    double ( *read )( x86_energy_single_counter_t t ); /** < Read the current energy value in Joules, will return < 0.0 on error */
    void ( *close )( x86_energy_single_counter_t t ); /** < Close a single counter */
    void ( *fini )( void ); /** < Finalize a source */
} x86_energy_access_source_t;


#endif /* INCLUDE_X86_ENERGY_SOURCE_H_ */
