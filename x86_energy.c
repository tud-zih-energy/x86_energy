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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "x86_energy.h"

/**
 * return the current time in microseconds
 */
uint64_t gettime_in_us(){
    struct timeval t;
    gettimeofday(&t,(struct timezone *)0);
    return t.tv_usec+t.tv_sec*1000000;
}

extern struct x86_energy_source rapl_source;
extern struct x86_energy_source amd_fam15_power_source;

static struct x86_energy_source * __get_available_sources(int threaded){
    /* If Intel and so on ... put Intel */
    if (!rapl_source.init(threaded))
        return &rapl_source;
    /* If AMD and activated ... put AMD */
    if (!amd_fam15_power_source.init(threaded))
        return &amd_fam15_power_source;
    return NULL;
}

/**
 * Trys to return either rapl or amd_fam15h_power source.
 * Spawns a background thread that periodically checks for overflows,
 * depending on the source detected.
 * If no suitable cpu is detected NULL is returned.
 */
struct x86_energy_source * get_available_sources(void)
{
    return __get_available_sources(1);
}

/**
 * Trys to return either rapl or amd_fam15h_power source.
 * Does not spawn a background thread for rapl.
 * For amd_fam15h_power the background thread is required for
 * energy measurement.
 * If no suitable cpu is detected NULL is returned.
 */
struct x86_energy_source * get_available_sources_nothread(void)
{
    return __get_available_sources(0);
}


/**
 * Returns the number of nodes in this system.
 * NOTE: On AMD_fam15h you should use get_nr_packages() from x86_energy_source.
 */
int x86_energy_get_nr_packages() {
    int n, nr_packages = 0;
    struct dirent **namelist;
    char *path = "/sys/devices/system/node";

    n = scandir(path, &namelist, NULL, alphasort);
    while(n--) {
        if(!strncmp(namelist[n]->d_name, "node", 4)) {
            nr_packages++;
        }
        free(namelist[n]);
    }
    free(namelist);

    return nr_packages;
}

/**
 * Returns the correponding node of the give cpu.
 * If the cpu can not be found on this system -1 is returned.
 */
int x86_energy_node_of_cpu(int __cpu) {
    int node;
    int nr_packages = x86_energy_get_nr_packages();
    char path[64];

    for(node = 0; node < nr_packages; node++)
    {
        size_t sz = snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpu%d", node, __cpu);
        assert(sz < sizeof(path));
        struct stat stat_buf;
        int stat_ret = stat(path, &stat_buf);
        if (0 == stat_ret)
        {
            return node;
        }
    }
    return -1;
}

