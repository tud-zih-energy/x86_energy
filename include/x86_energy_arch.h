/*
 * x86_energy_arch.h
 *
 *  Created on: 22.06.2018
 *      Author: rschoene
 */

#ifndef INCLUDE_X86_ENERGY_ARCH_H_
#define INCLUDE_X86_ENERGY_ARCH_H_

#include "x86_energy_source.h"

/* Describes granularities within the system
 * will be
 * system GRANULARITY_SYSTEM
 * -> package GRANULARITY_SOCKET
 *   -> node GRANULARITY_DIE
 *      -> module GRANULARITY_MODULE
 *        -> core GRANULARITY_CORE
 *          -> thread GRANULARITY_THREAD
 *   -> device GRANULARITY_DEVICE (TODO)
 */
enum x86_energy_granularity
{
    X86_ENERGY_GRANULARITY_SYSTEM,
    X86_ENERGY_GRANULARITY_SOCKET,
    X86_ENERGY_GRANULARITY_DIE,
    X86_ENERGY_GRANULARITY_MODULE,
    X86_ENERGY_GRANULARITY_CORE,
    X86_ENERGY_GRANULARITY_THREAD,
    X86_ENERGY_GRANULARITY_DEVICE, /** < Not supported yet */

    /* Non-ABI */
    X86_ENERGY_GRANULARITY_SIZE
};

/* will build a tree for the system */
typedef struct x86_energy_architecture_node x86_energy_architecture_node_t;

struct x86_energy_architecture_node {
    enum x86_energy_granularity granularity; /* granularity level of current node */
    long int id; /* id (unique for level) */
    char * name; /* a name that can be printed */
    size_t nr_children; /* number of sub nodes, will be 0 for X86_ENERGY_GRANULARITY_THREAD */
    x86_energy_architecture_node_t * children;
};

/* initialize nodes */
x86_energy_architecture_node_t * x86_energy_init_architecture_nodes( void );

/* free nodes */
void x86_energy_free_architecture_nodes( x86_energy_architecture_node_t * root );

/* find package/socket/... for OS CPU (X86_ENERGY_GRANULARITY_THREAD) */
x86_energy_architecture_node_t * x86_energy_find_arch_for_cpu(x86_energy_architecture_node_t * root, int granularity, int cpu);

/* count the number of, for example, sockets */
int x86_energy_arch_count(x86_energy_architecture_node_t * root, enum x86_energy_granularity granularity);

/* Print the tree to stdout */
void x86_energy_print(x86_energy_architecture_node_t * root,int lvl);

/**
 * Defines available features at current system
 */
typedef struct x86_energy_mechanism_struct {
    char * name; /** < Will tell you which mechanism is accessible, each mechanism has different implementations for access (sources) */
    enum x86_energy_granularity source_granularities[X86_ENERGY_COUNTER_SIZE]; /** < will tell you the granularity of each source, if it is >= X86_ENERGY_GRANULARITY_SIZE, it is not available */
    size_t nr_avail_sources; /** < length of avail_sources */
    struct x86_energy_access_source * avail_sources; /** < holds a list of possible interfaces to access your mechanism, just try them ;) */
}x86_energy_mechanisms_t;

/**
 * Gets the available mechanism on your system, including a description.
 */
x86_energy_mechanisms_t * x86_energy_get_avail_mechanism(void);



#endif /* INCLUDE_X86_ENERGY_ARCH_H_ */
