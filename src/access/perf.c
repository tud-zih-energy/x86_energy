/*
 * perf.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#define _GNU_SOURCE

#include <getopt.h>
#include <inttypes.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <asm/unistd.h>
#include <linux/perf_event.h>

#include "../include/access.h"
#include "../include/architecture.h"
#include "../include/error.h"

static char* strings_for_events[X86_ENERGY_COUNTER_SIZE] = {
    "pkg", "cores", "ram", "gpu", "psys",
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
static int get_event_id(char* suffix)
{
    char file_name_buffer[1024];
    int ret = snprintf(file_name_buffer, 1024,
                       "/sys/bus/event_source/devices/power/events/energy-%s", suffix);
    if (ret < 0 || ret == 1024)
    {
    	if(ret < 0) X86_ENERGY_SET_ERROR("output error while trying to assemble sysfs-path-string");
    	else        X86_ENERGY_SET_ERROR("specified suffix was too long");
        return -1;
    }
    FILE* fp = fopen(file_name_buffer, "r");
    if (fp == NULL)
    {
    	X86_ENERGY_SET_ERROR("could not obtain file pointer to \"%s\"" , file_name_buffer);
        return -1;
    }
    char* buffer = NULL;
    size_t len = 0;
    int read = getline(&buffer, &len, fp);
    fclose(fp);
    if (read <= 0)
    {
    	X86_ENERGY_SET_ERROR("could not read any bytes from \"%s\"", file_name_buffer);
        return -1;
    }
    unsigned int result;
    if (sscanf(buffer, "event=0x%xi", &result) != 1)
    {
    	X86_ENERGY_SET_ERROR("invalid content in file \"%s\", does not conform with mask event=0xFFFF", file_name_buffer);
        return 1;
    }
    return result;
}
/* returns < 0 as failure */
static double get_event_unit(char* suffix)
{
    char file_name_buffer[1024];
    int ret = snprintf(file_name_buffer, 1024,
                       "/sys/bus/event_source/devices/power/events/energy-%s.scale", suffix);
    if (ret < 0 || ret == 1024)
    {
    	if(ret < 0) X86_ENERGY_SET_ERROR("output error while trying to assemble sysfs-path-string");
    	else        X86_ENERGY_SET_ERROR("specified suffix was too long");
        return -1;
    }
    FILE* fp = fopen(file_name_buffer, "r");
    if (fp == NULL)
    {
    	X86_ENERGY_SET_ERROR("could not obtain file pointer to \"%s\"" , file_name_buffer);
        return -1;
    }
    double scale;
    if (fscanf(fp, "%le", &scale) != 1)
    {
        fclose(fp);
        X86_ENERGY_SET_ERROR("invalid content in file \"%s\", not a floating point number", file_name_buffer);
        return -1;
    }
    fclose(fp);
    return scale;
}

static int init(void)
{
    /* try to find event source and pkg event, which should be there always */
    if (get_event_id("pkg") < 0)
    {
    	X86_ENERGY_APPEND_ERROR("event id \"pkg\" not accessible");
        return 1;
    }

    /* try to find power perf type */
    FILE* fp = fopen("/sys/bus/event_source/devices/power/type", "r");
    if (fp == NULL)
    {
    	X86_ENERGY_SET_ERROR("could not obtain file pointer to file \"/sys/bus/event_source/devices/power/type\"");
        return 1;
    }
    if (fscanf(fp, "%d", &type) != 1)
    {
        fclose(fp);
        X86_ENERGY_SET_ERROR("file \"/sys/bus/event_source/devices/power/type\" does not contain a single number");
        return 1;
    }
    fclose(fp);
    return 0;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index)
{
    int cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);

    switch (counter_type)
    {
    case X86_ENERGY_COUNTER_PCKG:  /* fall-through */
    case X86_ENERGY_COUNTER_CORES: /* fall-through */
    case X86_ENERGY_COUNTER_DRAM:  /* fall-through */
    case X86_ENERGY_COUNTER_GPU:   /* fall-through */
    case X86_ENERGY_COUNTER_PLATFORM:
        break;
    default:
        X86_ENERGY_SET_ERROR("can't handle counter_type %d", counter_type);
        return NULL;
    }

    char* suffix = strings_for_events[counter_type];
    if (suffix == NULL)
    {
        X86_ENERGY_SET_ERROR("can't handle counter type counter size");
        return NULL;
    }

    int event_id = get_event_id(suffix);
    if (event_id < 0)
    {
    	X86_ENERGY_SET_ERROR("could not obtain event_id for event suffix \"%s\"", suffix);
        return NULL;
    }
    double unit = get_event_unit(suffix);
    if (unit < 0.0)
    {
    	X86_ENERGY_SET_ERROR("could not read unit for event suffix \"%s\"", suffix);
        return NULL;
    }

    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.type = type;
    attr.config = event_id;
    int fd = syscall(__NR_perf_event_open, &attr, -1, cpu, -1, 0);
    if (fd <= 0)
    {
    	X86_ENERGY_SET_ERROR("could not perform syscall to perf_event_open (pid=-1, cpu=%d)", cpu);
        return NULL;
    }

    struct reader_def* def = malloc(sizeof(struct reader_def));
    if (def == NULL)
    {
    	X86_ENERGY_SET_ERROR("could not allocate %d bytes of memory", sizeof(struct reader_def));
        return NULL;
    }
    uint64_t reading;
    if (read(fd, &reading, 8) != 8)
    {
        close(fd);
        X86_ENERGY_SET_ERROR("could not read the first 8 bytes from perf_event_open stream");
        return NULL;
    }
    def->fd = fd;
    def->cpuId = cpu;
    def->unit = unit;
    return (x86_energy_single_counter_t)def;
}

static double do_read(x86_energy_single_counter_t counter)
{
    uint64_t reading;
    struct reader_def* def = (struct reader_def*)counter;
    if (read(def->fd, &reading, 8) != 8)
    {
    	X86_ENERGY_SET_ERROR("could not read 8 bytes");
        return -1.0;
    }
    return reading * def->unit;
}

static void do_close(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    close(def->fd);
    free(def);
}
static void fini(void)
{
}

x86_energy_access_source_t perf_source = {.name = "perf-rapl",
                                          .init = init,
                                          .setup = setup,
                                          .read = do_read,
                                          .close = do_close,
                                          .fini = fini };
