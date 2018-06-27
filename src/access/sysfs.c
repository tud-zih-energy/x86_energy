/*
 * sysfs.c
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
#include <errno.h>
#include <pthread.h>

#include "../include/architecture.h"
#include "../include/access.h"
#include "../include/overflow_thread.h"

#define RAPL_PATH "/sys/class/powercap"

static char * sysfs_names [X86_ENERGY_COUNTER_SIZE] = {
        "package",
        "core",
        "dram",
        "uncore",
        "psys"
};

struct reader_def
{
    FILE * fp;
    int package;
    long long int last_reading;
    long long int overflow;
    long long int max;
    int cpu;
    pthread_t thread;
    pthread_mutex_t mutex;
};

static struct ov_struct sysfs_ov;

static double do_read( x86_energy_single_counter_t  counter);

static int init()
{
    DIR *test = opendir(RAPL_PATH);
    if (test != NULL) {
        closedir(test);
        return 0;
    }
    return 1;
}

static x86_energy_single_counter_t setup(enum x86_energy_counter counter_type, size_t index) {
    int cpu=get_test_cpu(X86_ENERGY_GRANULARITY_SOCKET, index);
    if ( cpu < 0 )
        return NULL;
    switch(counter_type)
    {
    case X86_ENERGY_COUNTER_PCKG: /* fall-through */
    case X86_ENERGY_COUNTER_CORES: /* fall-through */
    case X86_ENERGY_COUNTER_DRAM: /* fall-through */
    case X86_ENERGY_COUNTER_GPU: /* fall-through */
    case X86_ENERGY_COUNTER_PLATFORM:
        break
    default: return;
    }
    if (counter_type == X86_ENERGY_COUNTER_SIZE) return NULL;
    int given_package=index;
    char * name=sysfs_names[counter_type];
    struct dirent **namelist;
    int n, ret, total_files;
    char file_name_buffer[2048];
    FILE * final_fp= NULL;
    long long int final_max=-1;

    DIR *test = opendir(RAPL_PATH);
    if (test != NULL) {
        closedir(test);

        n = total_files = scandir(RAPL_PATH, &namelist, NULL, alphasort);
        while(n--) {
            int package;
            int dummy;
            int read_items=sscanf(namelist[n]->d_name, "intel-rapl:%d:%d",&package,&dummy);
            if ( read_items <= 0 )
                continue;
            if (package != given_package )
                continue;
            sprintf(file_name_buffer,"%s/%s/name",RAPL_PATH,namelist[n]->d_name);
            FILE * fp=fopen(file_name_buffer, "r");
            if ( fp == NULL )
                break;
            char * buffer=NULL;
            size_t len=0;
            int read = getline(&buffer, &len, fp);
            fclose(fp);
            if (read <=0)
            {
                break;
            }
            // remove \n
            buffer[strlen(buffer)-1]='\0';
            // packages have indices
            if (strncmp(buffer,"package",7) == 0 )
                buffer[7]='\0';
            if (strcmp(name,buffer)==0)
            {
                sprintf(file_name_buffer,RAPL_PATH"/%s/energy_uj",namelist[n]->d_name);
                final_fp=fopen(file_name_buffer, "r");
                if ( final_fp <= 0 )
                    break;
                sprintf(file_name_buffer,RAPL_PATH"/%s/max_energy_range_uj",namelist[n]->d_name);
                fp=fopen(file_name_buffer, "r");
                if ( fp == NULL )
                    break;
                long long int max;
                int read = fscanf(fp,"%llu",&max);
                fclose(fp);
                if (read == 0)
                    break;
                final_max=max;
                break;
            }
        }
        for (n=0;n<total_files;n++)
            free(namelist[n]);
        free(namelist);
    }
    else
        return NULL;

    if (final_fp == NULL )
        return NULL;
    if (final_max == -1)
    {
        fclose(final_fp);
        return NULL;
    }
    long long int last_reading;
    ret = fscanf(final_fp,"%llu",&last_reading);
    if ( ret < 1 )
    {
        fclose(final_fp);
        return NULL;
    }
    if (fseek(final_fp,0,SEEK_SET) != 0)
    {
        fclose(final_fp);
        return NULL;
    }
    if ( fflush(final_fp) != 0 )
    {
        fclose(final_fp);
        return NULL;
    }

    struct reader_def * def = malloc(sizeof(struct reader_def));
    def->fp=final_fp;
    def->cpu=cpu;
    def->max=final_max;
    def->package=given_package;
    def->last_reading=last_reading;
    def->overflow=0;
    if (x86_energy_overflow_thread_create(&sysfs_ov,cpu,&def->thread,&def->mutex,do_read,def, 30000000))
    {
        fclose(final_fp);
        free(def);
        return NULL;
    }
    return def;
}

static double do_read( x86_energy_single_counter_t  counter)
{
    struct reader_def * def = (struct reader_def *) counter;
    long long int reading;
    pthread_mutex_lock(&def->mutex);
    int ret = fscanf(def->fp,"%llu",&reading);
    if (fseek(def->fp,0,SEEK_SET) != 0)
    {
        return -1.0;
    }
    if (fflush(def->fp) != 0)
    {
        return -1.0;
    }
    if ( ret < 1 )
        return -1.0;
    if (reading < def->last_reading  )
    {
        def->overflow += 1;
    }
    def->last_reading = reading;
    pthread_mutex_unlock(&def->mutex);

    return 1.0E-6*(def->overflow*def->max+ def->last_reading);
}

static void do_close( x86_energy_single_counter_t counter )
{
    struct reader_def * def = (struct reader_def *) counter;
    x86_energy_overflow_thread_remove_call(&sysfs_ov,def->cpu,do_read,counter);
    fclose(def->fp);
    free(def);
}


static void fini(  )
{
    x86_energy_overflow_thread_killall(&sysfs_ov);
}

x86_energy_access_source_t sysfs_source =
{
    .name="sysfs-powercap-rapl",
    .init=init,
    .setup=setup,
    .read=do_read,
    .close=do_close,
    .fini=fini
};
