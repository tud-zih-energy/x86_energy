/*
 * likwid.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <likwid.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../include/access.h"
#include "../include/architecture.h"
#include "../include/error.h"
#include "../include/overflow_thread.h"

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

static long int* initialized_cpus;
static long int initialized_cpus_length;

static int is_likwid_initialized(long int cpu)
{
    for (long int i = 0; i < initialized_cpus_length; i++)
        if (initialized_cpus[i] == cpu)
            return true;
    return false;
}

static int add_likwid_initialize(long int cpu)
{
    if (is_likwid_initialized(cpu))
    {
        return 0;
    }
    long int* tmp = realloc(initialized_cpus, (initialized_cpus_length + 1) * sizeof(long int));
    if (tmp == NULL)
    {
        X86_ENERGY_SET_ERROR("Could not realloc likwid initialized CPUs");
        return 1;
    }
    initialized_cpus = tmp;
    initialized_cpus[initialized_cpus_length] = cpu;
    initialized_cpus_length++;
    return 0;
}

static double do_read(x86_energy_single_counter_t counter);

static int init(void)
{
    memset(&likwid_ov, 0, sizeof(struct ov_struct));
    int ret;
    HPMmode(ACCESSMODE_DAEMON);
    ret = topology_init();
    if (ret)
    {
        X86_ENERGY_SET_ERROR("during likwid topology_init");
        return 1;
    }
    ret = HPMinit();
    if (ret)
    {
        X86_ENERGY_SET_ERROR("during likwid HPMinit");
        return 1;
    }

    ret = power_init(0);
    if (ret == 0)
    {
        X86_ENERGY_SET_ERROR("during likwid power_init: RAPL not supported");
        return 1;
    }

    ret = add_likwid_initialize(0);
    if (ret)
    {
        X86_ENERGY_APPEND_ERROR("in add_likwid_initialize");
        return 1;
    }

    PowerInfo_t info = get_powerInfo();
    if (info == NULL)
    {
        X86_ENERGY_SET_ERROR("during likwid get_powerInfo");
        return 1;
    }
    if (!info->hasRAPL)
    {
        X86_ENERGY_SET_ERROR("likwid info->hasRAPL: no RAPL support ");
        return 1;
    }
    return 0;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index)
{
    int cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
    if (cpu < 0)
    {
        X86_ENERGY_APPEND_ERROR("could not get CPU for socket%li", index);
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
        X86_ENERGY_SET_ERROR("Invalid call to likwid.c->setup counter_type= %li", index);
        return NULL;
    }
    uint32_t reading;

    if (!is_likwid_initialized(cpu))
    {
        if (HPMaddThread(cpu))
        {
            X86_ENERGY_SET_ERROR("Problem with HPMaddThread for CPU %li", cpu);
            return NULL;
        }

        int ret = add_likwid_initialize(cpu);
        if (ret)
        {
            X86_ENERGY_APPEND_ERROR("Problem with add_likwid_initialize for CPU %li", cpu);
            return NULL;
        }
    }

    if (power_read(cpu, reg, &reading))
    {
        X86_ENERGY_SET_ERROR("Problem with power_read on CPU %li", cpu);
        return NULL;
    }
    struct reader_def* def = malloc(sizeof(struct reader_def));
    if (def == NULL)
    {
        X86_ENERGY_SET_ERROR("Problem with malloc during setup()");
        return NULL;
    }
    def->reg = reg;
    def->cpuId = cpu;
    def->last_reading = reading;
    if (x86_energy_overflow_thread_create(&likwid_ov, cpu, &def->thread, &def->mutex, do_read, def,
                                          30000000))
    {
        X86_ENERGY_SET_ERROR("Error creating a thread for CPU %li", cpu);
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
        X86_ENERGY_SET_ERROR("Error calling power_read for CPU %li REG %li", def->cpuId, def->reg);
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

    free(initialized_cpus);
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
