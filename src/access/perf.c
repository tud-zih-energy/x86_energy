/*
 * perf.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <string.h>

#include <asm/unistd.h>
#include <linux/perf_event.h>

#include "../include/access.h"
#include "../include/possible_counters.h"
#include "../include/architecture.h"



static char * strings_for_events[X86_ENERGY_COUNTER_SIZE]=
{
        "pkg",
        "cores",
        "ram",
        "gpu",
        "sys",
};

struct reader_def
{
    int cpuId;
    uint64_t last_reading;
    uint64_t fd;
    double unit;
};

static int type = 0;

/* returns < 0 as failure */
static int get_event_id(char * suffix)
{
    char file_name_buffer[1024];
    int ret=snprintf(file_name_buffer,1024,"/sys/bus/event_source/devices/power/events/energy-%s",suffix);
    if (ret < 0 || ret ==1024)
        return -1;
    FILE * fp=fopen(file_name_buffer, "r");
    if ( fp == NULL )
        return -1;
    char * buffer=NULL;
    size_t len=0;
    int read = getline(&buffer, &len, fp);
    fclose(fp);
    if (read <=0)
    {
        return -1;
    }
    unsigned int result;
    if (sscanf(buffer,"event=0x%xi",&result) != 1 )
        return 1;
    return result;
}
/* returns < 0 as failure */
static double get_event_unit(char * suffix)
{
    char file_name_buffer[1024];
    int ret=snprintf(file_name_buffer,1024,"/sys/bus/event_source/devices/power/events/energy-%s.scale",suffix);
    if (ret < 0 || ret ==1024)
        return -1;
    FILE * fp=fopen(file_name_buffer, "r");
    if ( fp == NULL )
        return -1;
    double scale;
    if (fscanf(fp,"%le",&scale) != 1)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return scale;
}

static int init( void ){
    /* try to find event source and pkg event, which should be there always */
    if (get_event_id("pkg") < 0)
        return 1;

    /* try to find power perf type */
    FILE * fp=fopen("/sys/bus/event_source/devices/power/type", "r");
    if ( fp == NULL )
        return 1;
    if (fscanf(fp,"%d",&type) != 1 )
    {
        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}


static x86_energy_single_counter_t setup( enum x86_energy_counter counter_type, size_t index )
{
    int cpu=get_test_cpu(index);

    if (counter_type == X86_ENERGY_COUNTER_SIZE)
        return NULL;
    char * suffix=strings_for_events[counter_type];
    int event_id=get_event_id(suffix);
    if (event_id < 0)
        return NULL;
    double unit=get_event_unit(suffix);
    if (unit < 0.0)
        return NULL;

    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.type=type;
    attr.config=event_id;
    int fd= syscall(__NR_perf_event_open, &attr, -1, cpu, -1, 0);
    if ( fd <= 0 )
        return NULL;

    struct reader_def * def = malloc (sizeof(struct reader_def));
    if (def == NULL)
        return NULL;
    uint64_t reading;
    if (read(fd,&reading,8) != 8)
    {
        close(fd);
        return NULL;
    }
    def->fd=fd;
    def->cpuId=cpu;
    def->unit=unit;
    return (x86_energy_single_counter_t) def;
}

static double do_read( x86_energy_single_counter_t  counter)
{
    uint64_t reading;
    struct reader_def * def = (struct reader_def *) counter;
    if (read(def->fd,&reading,8) != 8)
    {
        return -1.0;
    }
    return reading*def->unit;
}

static void do_close( x86_energy_single_counter_t counter )
{
    struct reader_def * def = (struct reader_def *) counter;
    close(def->fd);
    free(def);
}
static void fini( void )
{

}


x86_energy_access_source_t perf_source =
{
    .name="perf-rapl",
    .init=init,
    .setup=setup,
    .read=do_read,
    .close=do_close,
    .fini=fini
};
