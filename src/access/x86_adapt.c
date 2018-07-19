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
#include "../include/overflow_thread.h"

#define BUFFER_SIZE 4096
#define POWER_UNIT_REGISTER "Intel_RAPL_Power_Unit"

static char* x86a_names[X86_ENERGY_COUNTER_SIZE] = { "Intel_RAPL_Pckg_Energy",
                                                     "Intel_RAPL_PP0_Energy",
                                                     "Intel_RAPL_RAM_Energy",
                                                     "Intel_RAPL_PP1_Energy" };

struct reader_def
{
    uint64_t last_reading;
    uint64_t reg;
    double unit;
    int device;
    int pkg;
    int cpu;
    pthread_t thread;
    pthread_mutex_t mutex;
};

static struct ov_struct x86a_ov;

static double do_read(x86_energy_single_counter_t counter);

static int init(void)
{

    int ret = x86_adapt_init();

    if (ret)
    {
        x86_energy_set_error_string("Error in %s:%d: while calling x86_adapt_init %d\n", __FILE__, __LINE__, ret);
        return 1;
    }

    /* search for core freq parameters */
    int xa_index = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, x86a_names[0]);
    if (xa_index < 0)
    {
        x86_energy_set_error_string("Error in %s:%d: while calling x86_adapt_lookup_ci_name %s %d\n", __FILE__, __LINE__, x86a_names[0], ret);
        return 1;
    }
    return 0;
}

static double get_dram_unit()
{
    static double dram_unit = -1.0;
    if (dram_unit > 0.0)
        return dram_unit;
    unsigned int eax = 1, ebx = 0, ecx = 0, edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);
    if (FAMILY(eax) != 6)
        return 0;
    switch ((EXT_MODEL(eax) << 4) + MODEL(eax))
    {
    case 0x3f: /* Haswell-EP, fall-through */
    case 0x4e: /* Broadwell-EP, fall-through */
    case 0x55: /* SKL SP*/
        dram_unit = 1.0 / pow(2.0, 16.0);
        break;
    /* none of the above */
    default:
        return -1.0;
    }
    return dram_unit;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index)
{
    switch (counter_type)
    {
    case X86_ENERGY_COUNTER_PCKG:  /* fall-through */
    case X86_ENERGY_COUNTER_CORES: /* fall-through */
    case X86_ENERGY_COUNTER_DRAM:  /* fall-through */
    case X86_ENERGY_COUNTER_GPU:   /* fall-through */
        break;
    default:
        x86_energy_set_error_string("Error in %s:%d: Invalid call to x86_adapt.c->setup counter_type= %d\n", __FILE__, __LINE__, counter_type );
        return NULL;
    }

    int cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
    if (cpu < 0)
    {
        x86_energy_append_error_string("Error in %s:%d: calling get_test_cpu for socket %d\n", __FILE__, __LINE__, index );
        return NULL;
    }
    char* name = x86a_names[counter_type];
    if (name == NULL)
    {
        x86_energy_set_error_string("Error in %s:%d: setup counter_type %d not supported\n", __FILE__, __LINE__, index );
        return NULL;
    }

    int xa_index = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, name);
    if (xa_index < 0)
    {
        x86_energy_set_error_string("Error in %s:%d: setup Error calling x86_adapt_lookup_ci_name for %s %d\n", __FILE__, __LINE__, name, xa_index );
        return NULL;
    }

    int fd = x86_adapt_get_device_ro(X86_ADAPT_DIE, index);
    if (fd <= 0)
    {
        x86_energy_set_error_string("Error in %s:%d: setup Error calling x86_adapt_get_device_ro for package %d %d\n", __FILE__, __LINE__, index, fd );
        return NULL;
    }

    int xa_index_unit = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, POWER_UNIT_REGISTER);
    if (xa_index_unit < 0)
    {
        x86_energy_set_error_string("Error in %s:%d: setup Error calling x86_adapt_lookup_ci_name for power unit %d\n", __FILE__, __LINE__, name, xa_index );
        close(fd);
        x86_adapt_put_device(X86_ADAPT_DIE, index);
        return NULL;
    }

    double modifier_dbl = -1.0;
    if (counter_type == X86_ENERGY_COUNTER_DRAM)
        modifier_dbl = get_dram_unit();

    if (modifier_dbl < 0.0)
    {
        uint64_t modifier_u64;
        if (x86_adapt_get_setting(fd, xa_index_unit, &modifier_u64) != 8)
        {
            close(fd);
            x86_adapt_put_device(X86_ADAPT_DIE, index);
            return NULL;
        }

        modifier_u64 &= 0x1F00;
        modifier_u64 = modifier_u64 >> 8;
        modifier_dbl = modifier_u64;
        modifier_dbl = 1.0 / pow(2.0, modifier_dbl);
    }

    uint64_t current_setting;
    if (x86_adapt_get_setting(fd, xa_index, &current_setting) != 8)
    {
        x86_energy_set_error_string("Error in %s:%d: setup Error calling x86_adapt_get_setting for packaged %d\n", __FILE__, __LINE__, index );
        close(fd);
        x86_adapt_put_device(X86_ADAPT_DIE, index);
        return NULL;
    }

    struct reader_def* def = malloc(sizeof(struct reader_def));
    def->reg = xa_index;
    def->last_reading = current_setting;
    def->cpu = cpu;
    def->unit = modifier_dbl;
    def->device = fd;
    def->pkg = index;
    if (x86_energy_overflow_thread_create(&x86a_ov, cpu, &def->thread, &def->mutex, do_read, def,
                                          30000000))
    {
        x86_energy_set_error_string("Error in %s:%d: setup Error creating a thread for cpu %d\n", __FILE__, __LINE__, cpu);
        free(def);
        x86_adapt_put_device(X86_ADAPT_DIE, index);
        return NULL;
    }
    return (x86_energy_single_counter_t)def;
}

static double do_read(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    uint64_t reading;
    if (x86_adapt_get_setting(def->device, def->reg, &reading) != 8)
    {
    	x86_energy_set_error_string("Error in %s:%d: could not retrieve 8 bytes from x86_adapt", __FILE__, __LINE__);
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

static void do_close(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    x86_energy_overflow_thread_remove_call(&x86a_ov, def->cpu, do_read, counter);
    x86_adapt_put_device(X86_ADAPT_DIE, def->pkg);
    free(def);
}
static void fini(void)
{
    x86_energy_overflow_thread_killall(&x86a_ov);
    x86_energy_overflow_freeall(&x86a_ov);
}

x86_energy_access_source_t x86a_source = {.name = "x86a-rapl",
                                          .init = init,
                                          .setup = setup,
                                          .read = do_read,
                                          .close = do_close,
                                          .fini = fini };
