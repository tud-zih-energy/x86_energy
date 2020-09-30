/*
 * x86_adapt.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <x86_adapt.h>

#include "../../include/x86_energy.h"
#include "../include/access.h"
#include "../include/architecture.h"
#include "../include/cpuid.h"
#include "../include/error.h"
#include "../include/overflow_thread.h"

#define BUFFER_SIZE 4096
#define POWER_UNIT_REGISTER "Intel_RAPL_Power_Unit"
#define PKG_REGISTER "Intel_RAPL_Pckg_Energy"
#define CORE_REGISTER "AMD_RAPL_Core_Energy"

struct reader_def
{
    uint64_t last_reading;
    uint64_t reg;
    double unit;
    int device;
    int pkg;
    int cpu;
    int is_per_core;
    pthread_t thread;
    pthread_mutex_t mutex;
};

static struct ov_struct x86a_ov;

static double x86a_23_do_read(x86_energy_single_counter_t counter);

static int x86a_23_init(void)
{
    memset(&x86a_ov, 0, sizeof(struct ov_struct));
    int ret = x86_adapt_init();

    if (ret)
        return 1;

    /* search for core freq parameters */
    int xa_index = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, PKG_REGISTER);
    if (xa_index < 0)
    {
        X86_ENERGY_SET_ERROR(
            "could not lookup x86 adapter index (necessary for core freq parameters)");
        return 1;
    }
    return 0;
}
/**
 * TODO fix, more a wild guess here
 */
static double x86a_23_get_default_unit()
{

    static double default_unit = -1.0;
    if (default_unit > 0.0)
        return default_unit;

    int xa_index_unit = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, POWER_UNIT_REGISTER);
    if (xa_index_unit < 0)
    {
        X86_ENERGY_SET_ERROR("could not lookup index of power unit from x86 adapt");
        return -1.0;
    }

    int fd = x86_adapt_get_device_ro(X86_ADAPT_DIE, 0);
    if (fd <= 0)
    {
        X86_ENERGY_SET_ERROR("could not get a file descriptor for the DIE from x86 adapt");
        return -1.0;
    }

    uint64_t modifier_u64;
    if (x86_adapt_get_setting(fd, xa_index_unit, &modifier_u64) != 8)
    {
        //    TODO: x86_adapt_put_device(X86_ADAPT_DIE, 0);
        X86_ENERGY_SET_ERROR("Could not read 8 bytes from x86 adapt");
        return -1.0;
    }
    //    TODO: x86_adapt_put_device(X86_ADAPT_DIE, 0);

    modifier_u64 &= 0x1F00;
    modifier_u64 = modifier_u64 >> 8;
    default_unit = modifier_u64;
    default_unit = 1.0 / pow(2.0, default_unit);
    return default_unit;
}

static x86_energy_single_counter_t x86a_23_setup(enum x86_energy_counter counter_type, size_t index)
{

    int cpu;
    int xa_index;
    int xa_type;
    int fd;
    switch (counter_type)
    {
    case X86_ENERGY_COUNTER_PCKG:
        cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
        if (cpu < 0)
        {
            X86_ENERGY_SET_ERROR("could not get a cpu with granularity socket");
            return NULL;
        }
        xa_index = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, PKG_REGISTER);
        xa_type = X86_ADAPT_DIE;
        if (xa_index < 0)
        {
            X86_ENERGY_SET_ERROR("could not get the index of PKG_REGISTER from x86 adapt");
            return NULL;
        }
        fd = x86_adapt_get_device_ro(X86_ADAPT_DIE, index);
        if (fd <= 0)
        {
            X86_ENERGY_SET_ERROR(
                "could not get a file descriptor for the DIE to read from x86 adapt");
            return NULL;
        }
        break;
    case X86_ENERGY_COUNTER_SINGLE_CORE:
        cpu = get_test_cpu(X86_ENERGY_GRANULARITY_CORE, index);
        if (cpu < 0)
        {
            X86_ENERGY_SET_ERROR("could not get a cpu with granularity core");
            return NULL;
        }
        xa_index = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, CORE_REGISTER);
        xa_type = X86_ADAPT_CPU;
        if (xa_index < 0)
        {
            X86_ENERGY_SET_ERROR("could not get the index of CORE_REGISTER from x86 adapt");
            return NULL;
        }
        fd = x86_adapt_get_device_ro(X86_ADAPT_CPU, cpu);
        if (fd <= 0)
        {
            X86_ENERGY_SET_ERROR(
                "could not get a file descriptor for the CPU to read from x86 adapt");
            return NULL;
        }
        break;
    default:
        X86_ENERGY_SET_ERROR("can't handle counter type %d", counter_type);
        return NULL;
    }

    double unit = x86a_23_get_default_unit();
    if (unit < 0.0)
    {
        //    TODO: x86_adapt_put_device
        /*if (xa_type == X86_ADAPT_DIE)
            x86_adapt_put_device(X86_ADAPT_DIE, index);
        else
            x86_adapt_put_device(X86_ADAPT_CPU, cpu);*/
        X86_ENERGY_APPEND_ERROR("invalid unit");
        return NULL;
    }

    /* will happen for core */

    uint64_t current_setting;
    if (x86_adapt_get_setting(fd, xa_index, &current_setting) != 8)
    {
        //    TODO: x86_adapt_put_device
        /*if (xa_type == X86_ADAPT_DIE)
            x86_adapt_put_device(X86_ADAPT_DIE, index);
        else
            x86_adapt_put_device(X86_ADAPT_CPU, cpu);*/
        X86_ENERGY_SET_ERROR("could not read 8 bytes from x86 adapt file descriptor");
        return NULL;
    }

    struct reader_def* def = malloc(sizeof(struct reader_def));
    def->reg = xa_index;
    def->last_reading = current_setting;
    def->cpu = cpu;
    def->unit = unit;
    def->device = fd;
    switch (counter_type)
    {
    case X86_ENERGY_COUNTER_PCKG:
        def->is_per_core = 0;
        break;
    case X86_ENERGY_COUNTER_SINGLE_CORE:
        def->is_per_core = 1;
        break;
    default:
        free(def);
        //    TODO: x86_adapt_put_device
        /*if (xa_type == X86_ADAPT_DIE)
            x86_adapt_put_device(X86_ADAPT_DIE, index);
        else
            x86_adapt_put_device(X86_ADAPT_CPU, cpu);*/
        X86_ENERGY_SET_ERROR("can't handle counter type %d", counter_type);
        return NULL;
    }
    def->pkg = index;
    if (x86_energy_overflow_thread_create(&x86a_ov, cpu, &def->thread, &def->mutex, x86a_23_do_read, def,
                                          30000000))
    {
        free(def);
        X86_ENERGY_SET_ERROR("can't create thread related to cpu number %d", cpu);
        //    TODO: x86_adapt_put_device
        /*if (xa_type == X86_ADAPT_DIE)
            x86_adapt_put_device(X86_ADAPT_DIE, index);
        else
            x86_adapt_put_device(X86_ADAPT_CPU, cpu);*/
        return NULL;
    }
    return (x86_energy_single_counter_t)def;
}

static double x86a_23_do_read(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    uint64_t reading;
    if (x86_adapt_get_setting(def->device, def->reg, &reading) != 8)
    {
        X86_ENERGY_SET_ERROR("could not read 8 bytes from x86_adapt");
        return -1.0;
    }
    if (reading < (def->last_reading & 0xFFFFFFFFULL))
    {
        def->last_reading = (def->last_reading & 0xFFFFFFFF00000000ULL) + 0x100000000 + reading;
    }
    else
        def->last_reading = (def->last_reading & 0xFFFFFFFF00000000ULL) + reading;
    return def->unit * def->last_reading;
}

static void x86a_23_do_close(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    x86_energy_overflow_thread_remove_call(&x86a_ov, def->cpu, x86a_23_do_read, counter);

    //    TODO: x86_adapt_put_device
    /*if (def->per_core)
        x86_adapt_put_device(X86_ADAPT_CPU, cpu);
    else
        x86_adapt_put_device(X86_ADAPT_DIE, def->package);*/
    free(def);
}
static void x86a_23_fini(void)
{
    x86_energy_overflow_thread_killall(&x86a_ov);
    x86_energy_overflow_freeall(&x86a_ov);
}

x86_energy_access_source_t x86a_fam23_source = {.name = "x86a-rapl-amd",
                                                .init = x86a_23_init,
                                                .setup = x86a_23_setup,
                                                .read = x86a_23_do_read,
                                                .close = x86a_23_do_close,
                                                .fini = x86a_23_fini };
