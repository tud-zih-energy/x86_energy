/*
 * likwid.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <likwid.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/access.h"
#include "../include/architecture.h"
#include "../include/overflow_thread.h"
#include "../include/error.h"

#define MSR_PKG_ENERGY_STATUS 0x611
#define MSR_PP0_ENERGY_STATUS 0x639
#define MSR_PP1_ENERGY_STATUS 0x641
#define MSR_DRAM_ENERGY_STATUS 0x619
#define MSR_PLATFORM_ENERGY_STATUS 0x64D

struct reader_def
{
    int cpuId;
    uint64_t last_reading;
    uint64_t reg;
    pthread_t thread;
    pthread_mutex_t mutex;
    double unit;
};

static struct ov_struct likwid_ov;

static long int * initialized_cpus;
static long int initialized_cpus_length;

static int is_likwid_initialized(long int cpu)
{
    for ( long int i = 0; i < initialized_cpus_length; i++)
        if ( initialized_cpus[ i ] == cpu )
            return true;
    return false;
}

static int add_likwid_initialize(long int cpu)
{
    if ( is_likwid_initialized( cpu ) )
    {
        return 0;
    }
    long int * tmp = realloc( initialized_cpus, ( initialized_cpus_length +1 ) * sizeof (long int) );
    if ( tmp == NULL )
    {
        x86_energy_set_error_string( "Could not realloc likwid initialized CPUs\n" );
        return 1;
    }
    initialized_cpus = tmp;
    initialized_cpus [ initialized_cpus_length ] = cpu;
    initialized_cpus_length++;
    return 0;
}

static double do_read(x86_energy_single_counter_t counter);

static int init(void)
{
    int ret;
    HPMmode(ACCESSMODE_DAEMON);
    ret = topology_init();
    if (ret)
    {
        x86_energy_set_error_string( "Error during likwid topology_init\n" );
        return 1;
    }
    ret = HPMinit();
    if (ret)
    {
        x86_energy_set_error_string( "Error during likwid HPMinit\n" );
        return 1;
    }

    ret = power_init(0);
    if (ret == 0)
    {
        x86_energy_set_error_string( "Error during likwid power_init: RAPL not supported\n" );
        return 1;
    }

    ret = add_likwid_initialize( 0 );
    if ( ret )
    {
        x86_energy_append_error_string( "Error in add_likwid_initialize\n" );
        return 1;
    }

    PowerInfo_t info = get_powerInfo();
    if (info == NULL)
    {
        x86_energy_set_error_string( "Error during likwid get_powerInfo\n" );
        return 1;
    }
    if (!info->hasRAPL)
    {
        x86_energy_set_error_string( "likwid info->hasRAPL: no RAPL support \n" );
        return 1;
    }
    return 0;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index)
{
    int cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
    if (cpu < 0)
    {
        x86_energy_append_error_string( "likwid: could not get CPU for socket%li\n",index );
        return NULL;
    }
    uint64_t reg;
    PowerType domain;
    switch (counter_type)
    {
    case X86_ENERGY_COUNTER_PCKG:
        reg = MSR_PKG_ENERGY_STATUS;
        domain = PKG;
        break;
    case X86_ENERGY_COUNTER_CORES:
        reg = MSR_PP0_ENERGY_STATUS;
        domain = PP0;
        break;
    case X86_ENERGY_COUNTER_DRAM:
        reg = MSR_DRAM_ENERGY_STATUS;
        domain = DRAM;
        break;
    case X86_ENERGY_COUNTER_GPU:
        reg = MSR_PP1_ENERGY_STATUS;
        domain = PP1;
        break;
    case X86_ENERGY_COUNTER_PLATFORM:
        reg = MSR_PLATFORM_ENERGY_STATUS;
        domain = PLATFORM;
        break;
    default:
        x86_energy_set_error_string( "x86-energy likwid: Invalid call to likwid.c->setup counter_type= %li\n",index );
        return NULL;
    }
    uint32_t reading;

    if ( ! is_likwid_initialized( cpu ) )
    {
        if ( HPMaddThread( cpu ) )
        {
            x86_energy_set_error_string( "x86-energy likwid: Problem with HPMaddThread for CPU %li\n", cpu );
            return NULL;
        }

        int ret = add_likwid_initialize( cpu );
        if ( ret )
        {
            x86_energy_append_error_string( "x86-energy likwid: Problem with add_likwid_initialize for CPU %li\n", cpu );
            return NULL;
        }
    }

    if (power_read(cpu, reg, &reading))
    {
        x86_energy_set_error_string( "x86-energy likwid: Problem with power_read on CPU %li\n", cpu );
        return NULL;
    }
    struct reader_def* def = malloc(sizeof(struct reader_def));
    if (def == NULL)
    {
        x86_energy_set_error_string( "x86-energy likwid: Problem with malloc during setup()\n");
        return NULL;
    }
    def->reg = reg;
    def->cpuId = cpu;
    def->last_reading = reading;
    if (x86_energy_overflow_thread_create(&likwid_ov, cpu, &def->thread, &def->mutex, do_read, def,
                                          30000000))
    {
        x86_energy_set_error_string( "x86-energy likwid: Error creating a thread for CPU %li\n", cpu );
        free(def);
        return NULL;
    }

    def->unit = power_getEnergyUnit(domain);
    return (x86_energy_single_counter_t)def;
}

static double do_read(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    uint32_t reading;
    pthread_mutex_lock(&def->mutex);
    if (power_read(def->cpuId, def->reg, &reading))
    {
        x86_energy_set_error_string( "x86-energy likwid: Error calling power_read for CPU %li REG \n", def->cpuId, def->reg );
        pthread_mutex_unlock(&def->mutex);
        return -1.0;
    }
    if (reading < (def->last_reading & 0xFFFFFFFFULL))
    {
        def->last_reading = (def->last_reading & 0xFFFFFFFF00000000ULL) + 0x100000000 + reading;
    }
    else
        def->last_reading = (def->last_reading & 0xFFFFFFFF00000000ULL) + reading;
    pthread_mutex_unlock(&def->mutex);
    return def->unit * def->last_reading;
}

static void do_close(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    x86_energy_overflow_thread_remove_call(&likwid_ov, def->cpuId, do_read, counter);
}
static void fini(void)
{
    x86_energy_overflow_thread_killall(&likwid_ov);
    x86_energy_overflow_freeall(&likwid_ov);

    free( initialized_cpus );
    initialized_cpus = NULL;
    initialized_cpus_length = 0;

    power_finalize();
    topology_finalize();
}

x86_energy_access_source_t likwid_source = {.name = "likwid-rapl",
                                            .init = init,
                                            .setup = setup,
                                            .read = do_read,
                                            .close = do_close,
                                            .fini = fini };
