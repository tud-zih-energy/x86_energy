/*
 * sysfs.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/access.h"
#include "../include/architecture.h"
#include "../include/error.h"
#include "../include/overflow_thread.h"

#define RAPL_PATH "/sys/devices/virtual/powercap/intel-rapl"

static char* sysfs_names[X86_ENERGY_COUNTER_SIZE] = { "package", "core", "dram", "uncore", "psys", NULL };

struct reader_def
{
    FILE* fp;
    int package;
    long long int last_reading;
    long long int overflow;
    long long int max;
    int cpu;
    pthread_t thread;
    pthread_mutex_t mutex;
};

static struct ov_struct sysfs_ov;

static double do_read(x86_energy_single_counter_t counter);

static int init()
{
    memset(&sysfs_ov, 0, sizeof(struct ov_struct));
    DIR* test = opendir(RAPL_PATH);
    if (test != NULL)
    {
        int i, total_files, found_index=0;
        int package;
        int dummy;
        struct dirent** namelist;
        total_files = scandir(RAPL_PATH, &namelist, NULL, alphasort);
        if ( total_files > 0 )
        {
            for ( i = 0; i < total_files ; i++ )
            {
                int read_items = sscanf(namelist[i]->d_name, "intel-rapl:%d", &package);
                if (read_items > 0)
                {
                    break;
                }
            }
            if ( i < total_files )
                found_index = i;
            else
                found_index = total_files;
            for (i = 0; i < total_files; i++)
                free(namelist[i]);
            free(namelist);
        }
        closedir(test);
        if ( found_index < total_files )
            return 0;
        else
        {
            X86_ENERGY_SET_ERROR("No valid entries in RAPL_PATH (", RAPL_PATH," )");
            return 1;
        }
    }
    X86_ENERGY_SET_ERROR("RAPL_PATH (%s) can not be read", RAPL_PATH);
    return 1;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index)
{
    int cpu = get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
    if (cpu < 0)
    {
        X86_ENERGY_APPEND_ERROR("could not find a cpu with granularity socket");
        return NULL;
    }
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
    int given_package = index;
    char* name = sysfs_names[counter_type];
    struct dirent** namelist;
    int n, ret, total_files, read_items=0;
    char file_name_buffer[2048];
    FILE* final_fp = NULL;
    long long int final_max = -1;

    DIR* test = opendir(RAPL_PATH);
    if (test != NULL)
    {
        closedir(test);

        n = total_files = scandir(RAPL_PATH, &namelist, NULL, alphasort);

        while (n--)
        {
            int package;
            int dummy;
            // first we go through all
            // /sys/class/powercap/intel-rapl\:<N> and /sys/class/powercap/intel-rapl\:<N>:<M>
            read_items = sscanf(namelist[n]->d_name, "intel-rapl:%d", &package);
            if (read_items <= 0)
                continue;
            // now we verify that we are using the correct package in
            // /sys/class/powercap/intel-rapl\:<N>/name
            sprintf(file_name_buffer, "%s/%s/name", RAPL_PATH, namelist[n]->d_name);
            FILE* fp = fopen(file_name_buffer, "r");
            // could not open name file -> try next one
            if (fp == NULL)
                continue;

            // read name
            char* buffer = NULL;
            size_t len = 0;
            int read = getline(&buffer, &len, fp);
            fclose(fp);
            // could not read name? try next one
            if (read <= 0)
            {
                continue;
            }
            // remove \n
            buffer[strlen(buffer) - 1] = '\0';

            char final_folder[2048]="\0";

            switch (counter_type)
            {
            // psys
            case X86_ENERGY_COUNTER_PLATFORM:
            {
                if (strcmp(name, buffer) == 0)
                {
                    sprintf(final_folder, "%s/%s/", RAPL_PATH, namelist[n]->d_name);
                }
                break;
            }
            // package, cores, dram
            default:
            {
                // the content of RAPL_PATH/package-X/name should be "package-N"
                if (strncmp(buffer, "package", 7) != 0)
                    continue;
                // store for later
                int path_package=package;
                // try to read real package, starting at index 8
                errno = 0;
                package = strtol(&buffer[strlen("package-")], NULL, 10);
                if (package == 0 && errno != 0)
                    break;
                // not the package we were looking for? try next one
                if (package != given_package)
                    continue;

                // package we've been looking for! :)

                // only package in this folder, others in sub-folders ...
                if (counter_type != X86_ENERGY_COUNTER_PCKG)
                {
                    // will not really go to 100, but break before
                    for (int sub_item=0; sub_item<100;sub_item++)
                    {
                        sprintf(file_name_buffer, "%s/%s/%s:%d/name", RAPL_PATH, namelist[n]->d_name,namelist[n]->d_name,sub_item);
                        fp = fopen(file_name_buffer, "r");
                        if (fp == NULL)
                            break;

                        free(buffer);
                        buffer = NULL;
                        len = 0;
                        read = getline(&buffer, &len, fp);
                        fclose(fp);
                        if (read <= 0)
                        {
                            continue;
                        }
                        // remove \n
                        buffer[strlen(buffer) - 1] = '\0';
                        // if it is the searched counter
                        if (strcmp(name, buffer) == 0)
                        {
                            sprintf(final_folder, "%s/%s/%s:%d/", RAPL_PATH, namelist[n]->d_name,namelist[n]->d_name,sub_item);
                            break;
                        }
                    }
                }
                else {
                    sprintf(final_folder, "%s/%s/", RAPL_PATH, namelist[n]->d_name);

                }
                break;
            }
            }
            // now we should have a final folder, or its location is still "\0". check current values

            if (final_folder[0]!='\0')
            {
                sprintf(file_name_buffer, "%s/energy_uj", final_folder);
                final_fp = fopen(file_name_buffer, "r");
                if (final_fp <= 0)
                    break;
                sprintf(file_name_buffer, "%s/max_energy_range_uj", final_folder);
                fp = fopen(file_name_buffer, "r");
                if (fp == NULL)
                    break;
                long long int max;
                int read = fscanf(fp, "%llu", &max);
                fclose(fp);
                if (read == 0)
                    break;
                final_max = max;
                break;
            }
            free(buffer);
        }
        for (n = 0; n < total_files; n++)
            free(namelist[n]);
        free(namelist);
    }
    else
    {
        X86_ENERGY_SET_ERROR("can't open RAPL_PATH (%s)", RAPL_PATH);
        return NULL;
    }

    if (final_fp == NULL)
    {
        if ( read_items <= 0 )
        {
            X86_ENERGY_SET_ERROR("No valid files found in " RAPL_PATH);
            return NULL;
        }
        else
        {
            X86_ENERGY_SET_ERROR("could not get a file pointer to \"%s\"", file_name_buffer);
            return NULL;
        }
    }

    if (final_max == -1)
    {
        fclose(final_fp);
        X86_ENERGY_SET_ERROR("could not read any max_energy_range_uj");
        return NULL;
    }
    long long int last_reading;
    ret = fscanf(final_fp, "%llu", &last_reading);
    if (ret < 1)
    {
        fclose(final_fp);
        X86_ENERGY_SET_ERROR("contents in file \"%s\" do not conform to mask (unsigned long long)",
                             file_name_buffer);
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
        X86_ENERGY_SET_ERROR("could not flush file \"%s\"", file_name_buffer);
        return NULL;
    }

    struct reader_def* def = malloc(sizeof(struct reader_def));
    def->fp = final_fp;
    def->cpu = cpu;
    def->max = final_max;
    def->package = given_package;
    def->last_reading = last_reading;
    def->overflow = 0;
    if (x86_energy_overflow_thread_create(&sysfs_ov, cpu, &def->thread, &def->mutex, do_read, def,
                                          30000000))
    {
        fclose(final_fp);
        free(def);
        X86_ENERGY_SET_ERROR("could not open a thread related to cpu number %d", cpu);
        return NULL;
    }
    return def;
}

static double do_read(x86_energy_single_counter_t counter)
{
    struct reader_def* def = (struct reader_def*)counter;
    long long int reading;
    pthread_mutex_lock(&def->mutex);
    int ret = fscanf(def->fp, "%llu", &reading);
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
        X86_ENERGY_SET_ERROR(
            "contents of file related to cpu %d do not conform to mask (unsigned long long)",
            def->cpu);
        return -1.0;
    }
    if (reading < def->last_reading)
    {
        def->overflow += 1;
    }
    def->last_reading = reading;
    pthread_mutex_unlock(&def->mutex);

    return 1.0E-6 * (def->overflow * def->max + def->last_reading);
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

x86_energy_access_source_t sysfs_source = {.name = "sysfs-powercap-rapl",
                                           .init = init,
                                           .setup = setup,
                                           .read = do_read,
                                           .close = do_close,
                                           .fini = fini };
