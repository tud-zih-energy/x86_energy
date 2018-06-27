/**
 * x86_energy_arch.h
 *
 *  Created on: 22.06.2018
 *      Author: rschoene
 */

#ifndef INCLUDE_X86_ENERGY_H_
#define INCLUDE_X86_ENERGY_H_

#include <stddef.h>
#include <stdint.h>

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
    X86_ENERGY_GRANULARITY_SYSTEM, /**< The node represents the system (a Linux instance) */
    X86_ENERGY_GRANULARITY_SOCKET, /**< The node represents a physical processor */
    X86_ENERGY_GRANULARITY_DIE, /**< The node represents a NUM domain on the physical processor */
    X86_ENERGY_GRANULARITY_MODULE, /**< The node represents a module (a set of cores sharing L2) */
    X86_ENERGY_GRANULARITY_CORE, /**< The node represents a core */
    X86_ENERGY_GRANULARITY_THREAD, /**< The node represents a hardware thread (each core holds at least one), or a Linux CPU  */
    X86_ENERGY_GRANULARITY_DEVICE, /** < The node represents a device (not used yet) */

    /** Non-ABI, always compare to >= X86_ENERGY_GRANULARITY_SIZE instead of == , since this might change */
    X86_ENERGY_GRANULARITY_SIZE
};


/**
 * Enum for different types of energy counters
 */
enum x86_energy_counter
{
    X86_ENERGY_COUNTER_PCKG = 0, /**< one package / X86_ENERGY_GRANULARITY_SOCKET */
    X86_ENERGY_COUNTER_CORES = 1, /**< all cores of a package / X86_ENERGY_GRANULARITY_SOCKET */
    X86_ENERGY_COUNTER_DRAM = 2, /**< dram of one package / X86_ENERGY_GRANULARITY_SOCKET */
    X86_ENERGY_COUNTER_GPU = 3, /**< gpu of one package / X86_ENERGY_GRANULARITY_SOCKET */
    X86_ENERGY_COUNTER_PLATFORM = 4, /**< whole platform  / X86_ENERGY_GRANULARITY_SYSTEM */
    X86_ENERGY_COUNTER_SINGLE_CORE = 5, /**< a single core / X86_ENERGY_GRANULARITY_CORE */

    /* Non ABI */
    X86_ENERGY_COUNTER_SIZE
};

typedef struct x86_energy_architecture_node x86_energy_architecture_node_t;

/**
 * This struct is used to describe the underlying hardware
 */
struct x86_energy_architecture_node {
    enum x86_energy_granularity granularity; /**< granularity level of current node */
    int32_t id; /**< id (unique for level) */
    char * name; /**< a name that can be printed */
    size_t nr_children; /**< number of sub nodes, will be 0 for X86_ENERGY_GRANULARITY_THREAD */
    x86_energy_architecture_node_t * children; /**< the children, each system will have a number of sockets, ... */
};

/**
 * Create a tree representation of the current hardware
 * @return the hardware tree, NULL if error occured
 */
x86_energy_architecture_node_t * x86_energy_init_architecture_nodes( void );

/**
 *  Free a tree representation of the current hardware that was created with x86_energy_init_architecture_nodes
 *  @param root the tree gathered with x86_energy_init_architecture_nodes
 */
void x86_energy_free_architecture_nodes( x86_energy_architecture_node_t * root );

/**
 * Get hardware info for a specific CPU
 *
 * @param root the tree gathered with x86_energy_init_architecture_nodes
 * @param granularity the granularity to search for (e.g. find module for CPU 15 -> X86_ENERGY_GRANULARITY_MODULE)
 * @param cpu the CPU to ssearch the info for
 * @return the node with the given granularity that is in the graph between root and the cpu with id cpu
 */
x86_energy_architecture_node_t * x86_energy_find_arch_for_cpu(x86_energy_architecture_node_t * root, enum x86_energy_granularity granularity, int cpu);

/**
 *  count the number of, for example, sockets
 *
 *  @param root the tree gathered with x86_energy_init_architecture_nodes
 *  @param granularity  the type to count (e.g., count nr of cores: X86_ENERGY_GRANULARITY_CORE)
 */
int x86_energy_arch_count(x86_energy_architecture_node_t * root, enum x86_energy_granularity granularity);

/**
 *  Prints the tree to stdout
 *
 *  @param root the tree gathered with x86_energy_init_architecture_nodes
 *  @param level: number of spaces at begin of first entry
 */
void x86_energy_print(x86_energy_architecture_node_t * root,int lvl);

/**
 * Defines available features at current system
 */
typedef struct x86_energy_mechanism_struct {
    char * name; /** < Will tell you which mechanism is accessible (e.g. "Intel RAPL"), each mechanism has different implementations for access (sources), e.g. Likwid, sysfs */
    enum x86_energy_granularity source_granularities[X86_ENERGY_COUNTER_SIZE]; /**< will tell you the granularity for each possible counter, if it is >= X86_ENERGY_GRANULARITY_SIZE, it is not available */
    size_t nr_avail_sources; /** < length of avail_sources */
    struct x86_energy_access_source * avail_sources; /** < holds a list of possible interfaces to access your mechanism, just try them ;) */
}x86_energy_mechanisms_t;

/**
 * Gets the available mechanism on your system, including a description.
 */
x86_energy_mechanisms_t * x86_energy_get_avail_mechanism(void);

/**
 * Will be used by access sources
 */
typedef void * x86_energy_single_counter_t;

/**
 * One possible access source, e.g., likwid, perf, ...
 */
typedef struct x86_energy_access_source
{
    char * name; /**< The name of the source (e.g., Likwid, sysfs, perf) */
    int ( *init )( void ); /**< Call this for initialization, will return != 0 on error */
    x86_energy_single_counter_t ( *setup )( enum x86_energy_counter, size_t index ); /**< Call this for adding a counter, will return NULL on error */
    double ( *read )( x86_energy_single_counter_t t ); /**< Read the current energy value in Joules, will return < 0.0 on error */
    void ( *close )( x86_energy_single_counter_t t ); /**< Close a single counter */
    void ( *fini )( void ); /**< Finalize a source */
} x86_energy_access_source_t;



#endif /* INCLUDE_X86_ENERGY_H_ */
