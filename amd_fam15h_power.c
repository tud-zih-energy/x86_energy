/*
 libx86_energy.so, libx86_energy.a
 a library to count power and  energy consumption values on Intel SandyBridge and AMD Bulldozer
 Copyright (C) 2012 TU Dresden, ZIH

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License, v2.1, as
 published by the Free Software Foundation

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "x86_energy.h"

#define MILISECONDS10 10000

/* D18F3 */
#define REG_NORTHBRIDGE_CAPABILITIES 0xe8

/* D18F4 */
#define REG_PROCESSOR_TDP           0x1b8

/* D18F5 */
#define REG_TDP_RUNNING_AVERAGE     0xe0
#define REG_TDP_LIMIT3              0xe8

static int nr_packages=0;
static char nb[8][256];
struct per_nb_energy {
    int init;
    FILE *fd;
    uint64_t current_energy;   /* micro joule */
    uint64_t measure_time;
}__attribute__((aligned(64)));

struct per_die_fam15h {
    FILE *fd;
    uint64_t last_value;
}__attribute__((aligned(64)));

static struct per_nb_energy * energy_handles;
static struct per_die_fam15h * fam15h_handles;
static pthread_t thread;
static int thread_enabled;

/* for direct PCI access */
static int direct_pci=0;
static unsigned int tdp_to_watts=0;
static unsigned int base_tdp=0;

/**
 * amd_fam15h does not have any plattform specific features
 */
static struct plattform_features amd_features;

/**
 * Initializes the device of give package number.
 * Opens the corresponding file descriptor.
 */
static int amd_init_device(int package_nr){
    if(fam15h_handles == NULL || energy_handles == NULL) {
        fprintf(stderr, "X86_ENERGY: Run init BEFORE init_device\n");
    }

    fam15h_handles[package_nr].fd = fopen(nb[package_nr], "r");
    if(fam15h_handles[package_nr].fd == NULL) {
        fprintf(stderr,"X86_ENERGY: Could not get filedescriptor for package %d\n", package_nr);
        return -1;
    }
    setbuf(fam15h_handles[package_nr].fd, NULL);

    energy_handles[package_nr].fd = fopen(nb[package_nr], "r");
    if(energy_handles[package_nr].fd == NULL) {
        fprintf(stderr,"X86_ENERGY: Could not get filedescriptor for package %d\n", package_nr);
        return -1;
    }
    setbuf(energy_handles[package_nr].fd, NULL);
    /* intialize time values */
    energy_handles[package_nr].measure_time = gettime_in_us();

    energy_handles[package_nr].init = 1;

    return 0;
}

/**
 * Finalizes the device.
 * Closes the corresponding file descriptor.
 */
static int amd_fini_device(int package_nr){
    fclose(fam15h_handles[package_nr].fd);
    return 0;
}

static uint32_t __get_power(FILE *fd);

/**
 * Incremental counts the energy consumption.
 * Calculates every 10 miliseconds the current energy consumption
 * and aggregates it on a counter.
 */
static void * calculate_energy(void *ignore) {
    int i;
    uint64_t t1, diff;
    thread_enabled = 1;

    while(thread_enabled) {
        t1 = gettime_in_us();
        for(i=0;i<nr_packages;i++) {
            if(energy_handles[i].init) {
                /* incremental calculation */
                uint64_t last_time = energy_handles[i].measure_time;
                uint64_t data = __get_power(energy_handles[i].fd);
                energy_handles[i].measure_time = gettime_in_us();
                energy_handles[i].current_energy += data *
                    (energy_handles[i].measure_time - last_time) /
                    1000000; /* convert pico Joule to micro Joule */
            }
        }
        diff = gettime_in_us() - t1;

        if(diff < MILISECONDS10)
            usleep(MILISECONDS10 - diff);
        else
            fprintf(stderr,"X86_ENERGY Warning: Energy calculation took longer than 10 miliseconds\n");
    }
    /* close own file descriptor when thread is finished */
    for(i=0;i<nr_packages;i++)
        fclose(energy_handles[i].fd);

    return ignore;
}


/**
 * Initilizes the amd_fam15h_power plugin.
 * Checks for a suitable cpu.
 * Looks the northbridges paths up.
 * Counts the number of packages/
 * Allocates space for the handles.
 * Creates thread for counting the energy consumption
 * Note: the threaded parameter is ignored since energy-measurement
 * relies on this thread to be active.
 */
static int amd_init(int threaded) {
    struct dirent **namelist;
    int n;
    char *path = "/sys/module/fam15h_power/drivers/pci:fam15h_power";

    DIR *test = opendir("/sys/module/fam15h_power");
    if (test == NULL) {
        char b[256];
        uint32_t nr_nb,val,in0=0;
        FILE * f;
        int is_multinode;

        /* test failed */
        fprintf(stderr, "X86_ENERGY: CPU does not support fam15h_power, trying direct PCI access\n");

        /* check direct PCI access */
        for (nr_nb=0;nr_nb<16;nr_nb++){
            sprintf(b,"/proc/bus/pci/00/%x.5",0x18+nr_nb);
            f=fopen(b,"r");
            if (f==NULL) break;
            else fclose(f);
        }
        /* direct PCI access does not work -> return */
        if (nr_nb==0){
            fprintf(stderr, "X86_ENERGY: Could not open any PCI devices\n");
            return -1;
        }
        /* check if the cpu is a multi-node processor */
        f=fopen("/proc/bus/pci/00/18.3","r");
        if (pread(fileno(f),&val,4,REG_NORTHBRIDGE_CAPABILITIES)!=4){
            fprintf(stderr, "X86_ENERGY: Error reading northbridge capabilities\n");
            return -1;
        }
        is_multinode = (val >> 29) & 0x1;


        /* direct PCI access does work -> initialize */
        nr_packages=nr_nb;
        direct_pci=1;
        /* for multi-node socket */
        if (is_multinode) {
            for (int i=0;i<nr_packages;i++) {
                sprintf(b,"/proc/bus/pci/00/%x.3",0x18+i);
                f=fopen(b,"r");
                if (pread(fileno(f),&val,4,REG_NORTHBRIDGE_CAPABILITIES)!=4){
                    fprintf(stderr, "X86_ENERGY: Error reading northbridge capabilities\n");
                    return -1;
                }
                /* internal node0 */
                if (((val >> 30) & 0x3) == 0x0)
                    sprintf(nb[in0++],"/proc/bus/pci/00/%x.5",0x18+i);
            }
            /* only every 2nd node is needed for counting power consumption */
            nr_packages = nr_packages / 2;
        }
        /* for single-node other socket */
        else {
            for (int i=0;i<nr_packages;i++) {
                sprintf(nb[i],"/proc/bus/pci/00/%x.5",0x18+(i*2));
            }
        }

        /* get tdp stuff*/
        f=fopen("/proc/bus/pci/00/18.4","r");
        if (pread(fileno(f),&val,4,REG_PROCESSOR_TDP)!=4){
            fprintf(stderr, "X86_ENERGY: Error reading basic TDP (1b8)\n");
            return -1;
        }
        base_tdp = val >> 16;
        fclose(f);
        f=fopen("/proc/bus/pci/00/18.5","r");
        if (pread(fileno(f),&val,4,REG_TDP_LIMIT3)!=4){
            fprintf(stderr, "X86_ENERGY: Error reading basic TDP (e8)\n");
            return -1;
        }
        tdp_to_watts = ((val & 0x3ff) << 6) | ((val >> 10) & 0x3f);
        fclose(f);
    }
    else {
        /* test successfull */
        closedir(test);

        /* get northbridge paths */
        n = scandir(path, &namelist, NULL, alphasort);
        while(n--) {

            if(!strncmp(namelist[n]->d_name, "0000:00",7)) {
                sprintf(nb[nr_packages],
                    "/sys/module/fam15h_power/drivers/pci:fam15h_power/%s/power1_input",
                    namelist[n]->d_name);
                nr_packages++;
            }
           free(namelist[n]);
        }
        free(namelist);
    }

    /* allocatte space for handles */
    fam15h_handles = calloc(nr_packages, sizeof(struct per_die_fam15h));
    if(fam15h_handles == NULL){
        fprintf(stderr, "X86_ENERGY: Could NOT allocate memory for fam15h_handles\n");
        return -1;
    }
    energy_handles = calloc(nr_packages, sizeof(struct per_nb_energy));
    if(energy_handles == NULL){
        fprintf(stderr, "X86_ENERGY: Could NOT allocate memory for energy_handles\n");
        return -1;
    }

    /* create amd_features */
    amd_features.num = 1;
    amd_features.name = malloc(sizeof(char *));
    if (amd_features.name==NULL) {
        fprintf(stderr,"X86_ENERGY: Could NOT allocate memory for amd_features.name\n");
        return -1;
    }
    amd_features.ident = malloc(sizeof(int));
    if (amd_features.ident==NULL) {
        fprintf(stderr,"X86_ENERGY: Could NOT allocate memory for amd_features.ident\n");
        return -1;
    }
    amd_features.name[0] = strdup("package");
    amd_features.ident[0] = 0;

    /* create thread, that calculates the energy consumption */
    return pthread_create(&thread, NULL, &calculate_energy, NULL);
}

static inline int32_t sign_extend32(uint32_t value, int index)
{
  uint8_t shift = 31 - index;
  return (int32_t)(value << shift) >> shift;
}

/**
 * Reads the power consumption from a file descriptor.
 */
static inline uint32_t __get_power(FILE *fd){
    uint32_t data;
    if (direct_pci) {
      int32_t running_avg_capture;
      uint32_t running_avg_range , val,tdp_limit;
      uint64_t curr_pwr_watts;
      if (pread(fileno(fd),&val,4,REG_TDP_RUNNING_AVERAGE)!=4){
        fprintf(stderr, "X86_ENERGY: Error reading APM via PCI\n");
        return 0;
      }
      running_avg_capture = (val >> 4) & 0x3fffff;
      running_avg_capture = sign_extend32(running_avg_capture, 21);
      running_avg_range = (val & 0xf) + 1;
      if (pread(fileno(fd),&val,4,REG_TDP_LIMIT3)!=4) {
        fprintf(stderr, "X86_ENERGY: Error reading APM TDP via PCI\n");
        return 0;
      }
      tdp_limit = val >> 16;
      curr_pwr_watts = (tdp_limit + base_tdp) << running_avg_range;
      curr_pwr_watts -= running_avg_capture;
      curr_pwr_watts *= tdp_to_watts;
      /*
      * Convert to microWatt
      *
      * power is in Watt provided as fixed point integer with
      * scaling factor 1/(2^16).  For conversion we use
      * (10^6)/(2^16) = 15625/(2^10)
      */

      curr_pwr_watts = (curr_pwr_watts * 15625) >> (10 + running_avg_range);
      return curr_pwr_watts;

    } else {
      fscanf(fd,"%u",&data);
      rewind(fd);
    }
    return data;
}

/**
 * Gets the power consumption for a given package number.
 */
static double amd_get_power(int package_nr, int __ignore) {
    /* convert microwatt to watt */
    return __get_power(fam15h_handles[package_nr].fd)*0.000001;
}

/**
 * Gets the energy consumption for a given package number.
 */
static double amd_get_energy(int package_nr, int __ignore) {
    uint64_t data = energy_handles[package_nr].current_energy;

    /* convert pico joule to micro joule */
    return data * 0.000001;
}

static int amd_get_nr_packages(void) {
    return nr_packages;
}

/**
 * Finalizes the amd_fam15h_power plugin and waits for the energy counting thread to finish.
 */
static int amd_fini(void) {
    thread_enabled = 0;
    pthread_join(thread, NULL);

    free(amd_features.name[0]);
    free(amd_features.name);
    free(amd_features.ident);

    return 0;
}


struct x86_energy_source amd_fam15_power_source =
{
    GRANULARITY_SOCKET,
    amd_get_nr_packages,
    amd_init,
    amd_init_device,
    amd_fini_device,
    amd_fini,
    amd_get_power,
    amd_get_energy,
    &amd_features,
};
