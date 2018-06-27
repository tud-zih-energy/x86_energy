/*
 * msr_fam23.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include  <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include "../include/access.h"
#include "../include/cpuid.h"
#include "../include/architecture.h"
#include "../include/overflow_thread.h"

#define BUFFER_SIZE 4096

#define MSR_PWR_UNIT                    0xC0010299

#define MSR_PKG_ENERGY_STATUS           0xC001029B
#define MSR_CORE_ENERGY_STATUS          0xC001029A

struct reader_def
{
    int cpuId;
    uint64_t last_reading;
    uint64_t reg;
    pthread_t thread;
    pthread_mutex_t mutex;
    double unit;
};

static struct ov_struct msr_ov;

static double do_read( x86_energy_single_counter_t  counter);

/* this will return the maximal number of CPUs by looking for /dev/cpu/(nr)/msr[-safe]
 * It will also check whether these can be read
 * time complexity is O(num_cpus) for the first call. Afterwards its O(1), since the return value is buffered
 */
static int freq_gen_msr_get_max_entries(   )
{
    static long long int max = -1;
    if ( max != -1 )
    {
        return max;
    }
    char buffer[BUFFER_SIZE];
    DIR * dir = opendir("/dev/cpu/");
    if ( dir  == NULL)
    {
        return -EIO;
    }
    struct dirent * entry;

    while ( ( entry = readdir( dir ) ) != NULL )
    {
        if ( entry->d_type == DT_DIR )
        {
            /* first after cpu == numerical digit? */

            char* end;
            long long int current=strtoll(entry->d_name,&end,10);
            if ( end != ( entry->d_name + strlen(entry->d_name) ) )
                    continue;

            /* check access to msr */
            if (snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lli/msr",current) == BUFFER_SIZE)
            {
                closedir(dir);
                return -ENOMEM;
            }

            /* can not be accessed? check msr-safe */
            if (access (buffer, R_OK) != 0)
            {

                /* check access to msr */
                if (snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lli/msr-safe",current) == BUFFER_SIZE)
                {
                    closedir(dir);
                    return -ENOMEM;
                }
                if (access (buffer, R_OK) != 0)
                {
                    continue;
                }
            }

            if ( current > max )
                max = current;
        }
    }
    closedir(dir);
    if ( max == -1 )
        return -EACCES;
    max = max + 1;
    return max;
}

static int * fds;

/**
 * TODO fix, more a wild guess here
 */
static double get_default_unit(long unsigned cpu)
{

    static double default_unit = -1.0;
    if ( default_unit > 0.0 )
        return default_unit;
    int already_opened=fds[cpu];
    /* if not already open, open. if fail, return NULL */
    if ( fds[cpu] <=0 )
    {
        char buffer [BUFFER_SIZE];
        /* get uncore msr */

        if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lu/msr",cpu) == BUFFER_SIZE )
            return -1.0;

        fds[cpu] = open(buffer, O_RDONLY);
        if ( fds[cpu] < 0 )
        {
            if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lu/msr-safe",cpu) == BUFFER_SIZE )
                return -1.0;
            fds[cpu] = open(buffer, O_RDONLY);
            if ( fds[cpu] < 0 )
            {
                return -1.0;
            }
        }
    }
    uint64_t modifier_u64;
    int result=pread(fds[cpu],&modifier_u64,8,0xC0010299);

    /* close if was not open before*/
    if (already_opened <=0)
    {
        close(fds[cpu]);
        fds[cpu]=-1;
    }
    if (result != 8)
        return -1.0;


    modifier_u64 &= 0x1F00;
    modifier_u64 = modifier_u64 >> 8;
    default_unit = modifier_u64;
    default_unit = 1.0 / pow(2.0, default_unit);
    return default_unit;
}


static int init( void ){
    int max_msr=freq_gen_msr_get_max_entries();
    if (max_msr < 0 )
        return 1;
    fds=calloc(max_msr,sizeof(int));
    if (fds == NULL)
        return 1;
    return 0;
}


static x86_energy_single_counter_t setup( enum x86_energy_counter counter_type, size_t index )
{
    int cpu;
    uint64_t reg;
    switch (counter_type)
    {
    case    X86_ENERGY_COUNTER_PCKG:
        cpu=get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
        reg=MSR_PKG_ENERGY_STATUS;
        break;
    case    X86_ENERGY_COUNTER_SINGLE_CORE:
        cpu=get_test_cpu(X86_ENERGY_GRANULARITY_CORE, index);
        reg=MSR_CORE_ENERGY_STATUS;
        break;
/*    case    X86_ENERGY_COUNTER_SINGLE_CORE:
        cpu=get_test_cpu(X86_ENERGY_GRANULARITY_CORE, index);
        break;*/
    default:
        return NULL;
    }
    if ( cpu < 0 )
        return NULL;
    /* if not already open, open. if fail, return NULL */
    if ( fds[cpu] <=0 )
    {
        char buffer [BUFFER_SIZE];
        /* get uncore msr */

        if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lu/msr",cpu) == BUFFER_SIZE )
            return NULL;

        fds[cpu] = open(buffer, O_RDONLY);
        if ( fds[cpu] < 0 )
        {
            if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lu/msr-safe",cpu) == BUFFER_SIZE )
                return NULL;
            fds[cpu] = open(buffer, O_RDONLY);
            if ( fds[cpu] < 0 )
            {
                return NULL;
            }
        }
    }

    /* try to read */
    double unit=get_default_unit(cpu);
    int64_t reading;
    int result=pread(fds[cpu],&reading,8,reg);
    if (result!=8)
    {
        close(fds[cpu]);
        fds[cpu]=0;
        return NULL;
    }
    struct reader_def * def = malloc (sizeof(struct reader_def));
    if (def == NULL)
    {
        close(fds[cpu]);
        fds[cpu]=0;
        return NULL;
    }
    def->reg=reg;
    def->cpuId=cpu;
    def->last_reading=reading;
    def->unit=unit;
    if (x86_energy_overflow_thread_create(&msr_ov,cpu,&def->thread,&def->mutex,do_read,def, 30000000))
    {
        close(fds[cpu]);
        fds[cpu]=0;
        free(def);
        return NULL;
    }
    return (x86_energy_single_counter_t) def;
}


static double do_read( x86_energy_single_counter_t  counter)
{
    struct reader_def * def = (struct reader_def *) counter;
    uint64_t reading;
    pthread_mutex_lock(&def->mutex);
    int result=pread(fds[def->cpuId],&reading,8,def->reg);
    if (result != 8)
        return -1.0;
    reading = reading & 0xFFFFFFFF;
    if (reading < (def->last_reading & 0xFFFFFFFFULL) )
    {
        def->last_reading = (def->last_reading & 0xFFFFFFFF00000000ULL) + 0x100000000 + reading;
    }
    else
        def->last_reading = (def->last_reading & 0xFFFFFFFF00000000ULL) + reading;
    pthread_mutex_unlock(&def->mutex);
    return def->unit*def->last_reading;
}

static void do_close( x86_energy_single_counter_t counter )
{
    struct reader_def * def = (struct reader_def *) counter;
    x86_energy_overflow_thread_remove_call(&msr_ov,def->cpuId,do_read,counter);
    close(fds[def->cpuId]);
    fds[def->cpuId]=0;
    free(def);
}
static void fini( void )
{
    x86_energy_overflow_thread_killall(&msr_ov);
    x86_energy_overflow_freeall(&msr_ov);
}

x86_energy_access_source_t msr_fam23_source =
{
    .name="msr-rapl-fam23",
    .init=init,
    .setup=setup,
    .read=do_read,
    .close=do_close,
    .fini=fini
};
