/*
 * sysfs.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

/* experimental */
#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)


#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../include/access.h"
#include "../include/architecture.h"
#include "../include/overflow_thread.h"
#include "../include/error.h"

#define APM_PATH "/sys/module/fam15h_power/drivers/pci:fam15h_power/"
#define APM_PREFIX "/hwmon/hwmon"
#define APM_PREFIX2 "/power1_input"

struct reader_def
{
    FILE* fp;
    int package;
    struct timeval last_reading_tv;
    double energy;
    int cpu;
    pthread_t thread;
    pthread_mutex_t mutex;
};

static struct ov_struct sysfs_ov;

static x86_energy_architecture_node_t* arch_info;

static double do_read(x86_energy_single_counter_t counter);

static int init()
{
    DIR* test = opendir(APM_PATH);
    if (test != NULL)
    {
        closedir(test);
        arch_info = x86_energy_init_architecture_nodes();
        if (arch_info == NULL)
        {
        	X86_ENERGY_APPEND_ERROR("could not initialize architecture");
            return 1;
        }
        return 0;
    }
    X86_ENERGY_SET_ERROR("call to opendir(%s) returned NULL", APM_PATH);
    return 1;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index)
{
    int cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
    if (cpu < 0)
    {
    	X86_ENERGY_APPEND_ERROR("no cpu with granularity socket");
        return NULL;
    }
    if (counter_type != X86_ENERGY_COUNTER_PCKG)
    {
    	X86_ENERGY_SET_ERROR("can't handle any other counter_type than COUNTER_PCKG, counter type %d refused", counter_type);
        return NULL;
    }
    int given_package = index;
    struct dirent** namelist;
    int n, ret, total_files;
    char file_name_buffer[2048];
    FILE* final_fp = NULL;

    DIR* test = opendir(APM_PATH);
    if (test != NULL)
    {
        closedir(test);

        n = total_files = scandir(APM_PATH, &namelist, NULL, alphasort);
        while (n--)
        {
            int package;
            int line;
            int read_items = sscanf(namelist[n]->d_name, "0000:00:%x.%d", &package, &line);
            if (read_items <= 1)
                continue;
            // check cpu list to get package ...
            sprintf(file_name_buffer, APM_PATH "%s/local_cpulist", namelist[n]->d_name);
            FILE* fp = fopen(file_name_buffer, "r");
            if (fp == NULL)
                break;
            ret = fscanf(fp, "%d", &cpu);
            fclose(fp);
            if (ret != 1)
                break;
            x86_energy_architecture_node_t* package_node =
                x86_energy_find_arch_for_cpu(arch_info, X86_ENERGY_GRANULARITY_SOCKET, cpu);
            if (package_node == NULL || package_node->id != given_package)
                continue;

            sprintf(file_name_buffer, APM_PATH "/%s/" APM_PREFIX "%d" APM_PREFIX2,
                    namelist[n]->d_name, given_package);
            final_fp = fopen(file_name_buffer, "r");
            if (final_fp == NULL)
            {
            	X86_ENERGY_SET_ERROR("Error in "__FILE__":" S__LINE__ ": could not get a file pointer to \"%s\"", file_name_buffer);
                return NULL;
            }
        }
        for (n = 0; n < total_files; n++)
            free(namelist[n]);
        free(namelist);
    }
    else
    {
    	X86_ENERGY_SET_ERROR("could not read directory \"%s\" opendir returned NULL", APM_PATH);
        return NULL;
    }

    if (final_fp == NULL)
    {
    	X86_ENERGY_SET_ERROR("received NULL as file pointer to \"%s\"", file_name_buffer);
        return NULL;
    }

    long long int last_reading;
    ret = fscanf(final_fp, "%llu", &last_reading);
    if (ret < 1)
    {
        fclose(final_fp);
        X86_ENERGY_SET_ERROR("contents of file \"%s\" do not conform to mask (unsigned long long)", file_name_buffer);
        return NULL;
    }
    if (fseek(final_fp, 0, SEEK_SET) != 0)
    {
        fclose(final_fp);
        X86_ENERGY_SET_ERROR("could not seek to index 0 in file \"%s\"", file_name_buffer);
        return NULL;
    }
    if (fflush(final_fp) != 0)
    {
        fclose(final_fp);
        X86_ENERGY_SET_ERROR("could not flush to file \"%s\"", file_name_buffer);
        return NULL;
    }

    struct reader_def* def = malloc(sizeof(struct reader_def));
    def->fp = final_fp;
    def->cpu = cpu;
    def->package = given_package;
    def->energy = 0;
    gettimeofday(&def->last_reading_tv, NULL);
    if (x86_energy_overflow_thread_create(&sysfs_ov, cpu, &def->thread, &def->mutex, do_read, def,
                                          10000))
    {
        fclose(final_fp);
        free(def);
        X86_ENERGY_SET_ERROR("could not create thread for cpu %d", cpu);
        return NULL;
    }
    return def;
}

static double do_read(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    long long power_in_uW;
    pthread_mutex_lock(&def->mutex);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int ret = fscanf(def->fp, "%llu", &power_in_uW);
    if (fseek(def->fp, 0, SEEK_SET) != 0)
    {
    	pthread_mutex_unlock(&def->mutex);
        X86_ENERGY_SET_ERROR("could not seek to index 0 in file related to cpu %d", def->cpu);
        return -1.0;
    }
    if (fflush(def->fp) != 0)
    {
    	pthread_mutex_unlock(&def->mutex);
        X86_ENERGY_SET_ERROR("could not flush file related to cpu %d", def->cpu);
        return -1.0;
    }
    if (ret < 1)
    {
    	pthread_mutex_unlock(&def->mutex);
        X86_ENERGY_SET_ERROR("contents of file related to cpu %d do not conform to mask (unsigned long long)", def->cpu);
        return -1.0;
    }
    double time = 1E-6 * ((1000000 * tv.tv_sec) + tv.tv_usec - def->last_reading_tv.tv_usec -
                          (1000000 * def->last_reading_tv.tv_sec));
    double power = (double)1E-6 * power_in_uW;
    def->energy += time * power;
    def->last_reading_tv = tv;
    pthread_mutex_unlock(&def->mutex);

    return def->energy;
}

static void do_close(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    x86_energy_overflow_thread_remove_call(&sysfs_ov, def->cpu, do_read, counter);
    fclose(def->fp);
    free(def);
}

static void fini()
{
    x86_energy_overflow_thread_killall(&sysfs_ov);
}

x86_energy_access_source_t sysfs_fam15_source = {.name = "sysfs-Fam15h",
                                                 .init = init,
                                                 .setup = setup,
                                                 .read = do_read,
                                                 .close = do_close,
                                                 .fini = fini };
