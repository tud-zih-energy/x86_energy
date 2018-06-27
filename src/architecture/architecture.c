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
        if ( FAMILY(eax) + EXT_FAMILY(eax) == 15 )
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
        t->nr_avail_sources=3;
#ifdef USELIKWID
        t->nr_avail_sources+=1;
#endif
#ifdef USEX86_ADAPT
        t->nr_avail_sources+=1;
#endif
        t->avail_sources= malloc(t->nr_avail_sources*sizeof(x86_energy_access_source_t));

        int current=0;
        t->avail_sources[current++]=sysfs_source;
        t->avail_sources[current++]=perf_source;
        t->avail_sources[current++]=msr_source;
#ifdef USELIKWID
        t->avail_sources[current++]=likwid_source;
#endif
#ifdef USEX86_ADAPT
        t->avail_sources[current++]=x86a_source;
#endif
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

        // TODO msr
#ifdef USEX86_ADAPT
        // TODO x86a
#endif
        return t;
    }
    // TODO Fam 23


    return NULL;
}

static x86_energy_architecture_node_t * find_node_internal( x86_energy_architecture_node_t *current, enum x86_energy_granularity given_granularity, unsigned long int id)
{
    if ( current->granularity == given_granularity && current->id == id )
        return current;
    for (int i=0;i<current->nr_children;i++)
    {
        x86_energy_architecture_node_t * found = find_node_internal(&(current->children[i]),given_granularity,id);
        if (found != NULL)
            return found;
    }
    return NULL;
}

long get_test_cpu(enum x86_energy_granularity given_granularity, unsigned long int id)
{
    x86_energy_architecture_node_t * current = arch;

    x86_energy_architecture_node_t * sub_node = find_node_internal( current, given_granularity, id);

    if ( sub_node == NULL )
        return -1;

    while (sub_node->granularity != X86_ENERGY_GRANULARITY_THREAD)
        sub_node=sub_node->children;
    return sub_node->id;
}

