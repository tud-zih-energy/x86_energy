/*
 * architecture.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "../include/architecture.h"

#include "../../include/x86_energy.h"
#include "../include/access.h"
#include "../include/cpuid.h"


static x86_energy_architecture_node_t * arch;

x86_energy_mechanisms_t * x86_energy_get_avail_mechanism(void)
{
    arch = x86_energy_init_architecture_nodes();

    if (arch == NULL)
        return NULL;
    x86_energy_print(arch,0);
    int num_packages = x86_energy_arch_count(arch,X86_ENERGY_GRANULARITY_SOCKET);

    if (num_packages <= 0)
        return NULL;

    bool is_intel=false,is_amd=false;
    bool supported[X86_ENERGY_COUNTER_SIZE];
    for ( int i = 0 ; i < X86_ENERGY_COUNTER_SIZE ; i++ )
        supported[ i ] = false;

    char buffer[13];
    unsigned int eax = 0, ebx=0, ecx=0, edx=0;
    cpuid(&eax,&ebx,&ecx,&edx);

    /* reorder name string */
    buffer[0]=ebx & 0xFF;
    buffer[1]=(ebx>>8) & 0xFF;
    buffer[2]=(ebx>>16) & 0xFF;
    buffer[3]=(ebx>>24) & 0xFF;
    buffer[4]=edx & 0xFF;
    buffer[5]=(edx>>8) & 0xFF;
    buffer[6]=(edx>>16) & 0xFF;
    buffer[7]=(edx>>24) & 0xFF;
    buffer[8]=ecx & 0xFF;
    buffer[9]=(ecx>>8) & 0xFF;
    buffer[10]=(ecx>>16) & 0xFF;
    buffer[11]=(ecx>>24) & 0xFF;
    buffer[12]='\0';

    if (strcmp(buffer, "GenuineIntel") == 0 )
    {
        eax=1;
        cpuid(&eax,&ebx,&ecx,&edx);
        if ( FAMILY(eax) != 6 )
            return 0;
        switch ((EXT_MODEL(eax) << 4 )+ MODEL(eax))
        {
        /* Sandy Bridge */
        case 0x2a:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_GPU]=true;
            is_intel=true;
            break;
        case 0x2d:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        /* Ivy Bridge */
        case 0x3a:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_GPU]=true;
            is_intel=true;
            break;
        case 0x3e:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        /* Haswell */
        case 0x3c:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            supported[X86_ENERGY_COUNTER_GPU]=true;
            is_intel=true;
            break;
        case 0x45:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        case 0x46:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        case 0x3f:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        /* Broadwell*/
        case 0x3d:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            supported[X86_ENERGY_COUNTER_GPU]=true;
            is_intel=true;
            break;
        case 0x47:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_CORES]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        case 0x56:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        case 0x4f:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        /* Skylake*/
        case 0x4e:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            is_intel=true;
            break;
        case 0x55:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        case 0x5e:
            supported[X86_ENERGY_COUNTER_PCKG]=true;
            supported[X86_ENERGY_COUNTER_DRAM]=true;
            is_intel=true;
            break;
        /* none of the above */
        default:
            break;
        }
    }
    /* currently only Fam15h */
    else if (strcmp(buffer, "AuthenticAMD") == 0 )
    {
        eax=1;
        cpuid(&eax,&ebx,&ecx,&edx);
        if ( FAMILY(eax) == 15 )
        {
            is_amd=true;
            supported[X86_ENERGY_COUNTER_PCKG]=true;
        }
    }
    else
        return NULL;
    if ( is_intel )
    {
        x86_energy_mechanisms_t * t = malloc(sizeof(x86_energy_mechanisms_t));
        t->name="Intel RAPL";
        for (int i=0;i<X86_ENERGY_COUNTER_PLATFORM;i++)
            if (supported[i])
                t->source_granularities[i]=X86_ENERGY_GRANULARITY_SOCKET;
            else
                t->source_granularities[i]=X86_ENERGY_GRANULARITY_SIZE;
        if (supported[X86_ENERGY_COUNTER_PLATFORM])
            t->source_granularities[X86_ENERGY_COUNTER_PLATFORM]=X86_ENERGY_GRANULARITY_SYSTEM;
        else
            t->source_granularities[X86_ENERGY_COUNTER_PLATFORM]=X86_ENERGY_GRANULARITY_SIZE;
        t->nr_avail_sources=5;
        t->avail_sources= malloc(5*sizeof(x86_energy_access_source_t));
        t->avail_sources[0]=likwid_source;
        t->avail_sources[1]=msr_source;
        t->avail_sources[2]=sysfs_source;
        t->avail_sources[3]=perf_source;
        t->avail_sources[4]=x86a_source;
        return t;
    }
    if ( is_amd )
    {
        x86_energy_mechanisms_t * t = malloc(sizeof(x86_energy_mechanisms_t));
        t->name="AMD APM";
        for (int i=0;i<X86_ENERGY_COUNTER_SIZE;i++)
            if (supported[i])
                t->source_granularities[i]=X86_ENERGY_GRANULARITY_SOCKET;
            else
                t->source_granularities[i]=X86_ENERGY_GRANULARITY_SIZE;

        t->nr_avail_sources=1;
        t->avail_sources= malloc(1*sizeof(x86_energy_access_source_t));
        t->avail_sources[0]=sysfs_fam15_source;
        t->avail_sources[1]=procfs_fam15_source;
        return t;
    }

    return NULL;
}

long get_test_cpu(unsigned long int package)
{
    x86_energy_architecture_node_t * current = arch;
    while (current->granularity != X86_ENERGY_GRANULARITY_THREAD)
        current=current->children;
    return current->id;
}
