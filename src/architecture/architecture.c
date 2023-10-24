/*
 * architecture.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/architecture.h"

#include "../../include/x86_energy.h"
#include "../include/access.h"
#include "../include/cpuid.h"
#include "../include/error.h"


static x86_energy_architecture_node_t* arch;


static bool is_selected_source(x86_energy_access_source_t source)
{
    static bool env_initialized = false;
    static char * env_string = NULL;
    if ( !env_initialized )
    {
        env_string = getenv("X86_ENERGY_SOURCE");
        if ( env_string != NULL )
        {
            env_string =strdup( env_string );
            /* TODO check return value */
        }
        env_initialized = true;
    }
    if ( env_string == NULL )
    {
        return true;
    }
    if ( strcmp( env_string, source.name ) == 0 )
    {
        return true;
    }
    return false;
}

x86_energy_mechanisms_t* x86_energy_get_avail_mechanism(void)
{
    arch = x86_energy_init_architecture_nodes();

    if (arch == NULL)
    {
        X86_ENERGY_APPEND_ERROR("while calling x86_energy_init_architecture_nodes");
        return NULL;
    }
    int num_packages = x86_energy_arch_count(arch, X86_ENERGY_GRANULARITY_SOCKET);

    if (num_packages <= 0)
    {
        X86_ENERGY_APPEND_ERROR("while calling x86_energy_arch_count");
        return NULL;
    }

    bool is_intel = false, is_amd = false, is_amd_rapl = false;
    bool supported[X86_ENERGY_COUNTER_SIZE];
    for (int i = 0; i < X86_ENERGY_COUNTER_SIZE; i++)
        supported[i] = false;

    char buffer[13];
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);

    /* reorder name string */
    buffer[0] = ebx & 0xFF;
    buffer[1] = (ebx >> 8) & 0xFF;
    buffer[2] = (ebx >> 16) & 0xFF;
    buffer[3] = (ebx >> 24) & 0xFF;
    buffer[4] = edx & 0xFF;
    buffer[5] = (edx >> 8) & 0xFF;
    buffer[6] = (edx >> 16) & 0xFF;
    buffer[7] = (edx >> 24) & 0xFF;
    buffer[8] = ecx & 0xFF;
    buffer[9] = (ecx >> 8) & 0xFF;
    buffer[10] = (ecx >> 16) & 0xFF;
    buffer[11] = (ecx >> 24) & 0xFF;
    buffer[12] = '\0';



    int cpu_base_family = 0,
    	cpu_ext_family  = 0,
		cpu_base_model  = 0,
		cpu_ext_model   = 0,
		cpu_family      = 0,
		cpu_model       = 0;

    if(strcmp(buffer, "GenuineIntel") == 0 || strcmp(buffer, "AuthenticAMD") == 0)
    {
    	eax = 1;
    	cpuid(&eax, &ebx, &ecx, &edx);

        cpu_base_family = FAMILY(eax);
        cpu_ext_family  = EXT_FAMILY(eax);
        cpu_base_model  = MODEL(eax);
        cpu_ext_model   = EXT_MODEL(eax);

        /* Intel and AMD have suggested applications to display the family
         * of a CPU as the sum of the "Family" and the "Extended Family"
         * fields shown above, and the model as the sum of the "Model" and
         * the 4-bit left-shifted "Extended Model" fields.[5]
         *
         * If "Family" is different than 6 or 15, only the "Family" and "Model"
         * fields should be used while the "Extended Family" and
         * "Extended Model" bits are reserved.
         *
         * If "Family" is set to 15, then "Extended Family" and the 4-bit
         * left-shifted "Extended Model" should be added to the respective
         * base values, and if "Family" is set to 6, then only the 4-bit
         * left-shifted "Extended Model" should be added to "Model".[6][7]
         *
         * [5] http://download.intel.com/design/processor/applnots/24161832.pdf
         * [6] http://support.amd.com/us/Embedded_TechDocs/25481.pdf
         * [7] http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf
         *
         * Source: https://en.wikipedia.org/wiki/CPUID
         */

        if(6 == cpu_base_family)
        {
            cpu_model = (cpu_ext_model << 4) + cpu_base_model;
            cpu_family = cpu_base_family;
        } else if(15 == cpu_base_family)
        {
            cpu_model  = (cpu_ext_model  << 4) + cpu_base_model;
            cpu_family = cpu_ext_family + cpu_base_family;
        } else
        {
            cpu_model  = cpu_base_model;
            cpu_family = cpu_base_family;
        }

    } else
    {
        X86_ENERGY_SET_ERROR("the calling CPU is neither Intel, nor AMD");
        return NULL;
    }

    if (strcmp(buffer, "GenuineIntel") == 0)
    {
        if (cpu_family != 6)
        {
        	X86_ENERGY_SET_ERROR("Not a supported Intel processor family (family 0x%x, model 0x%x)", cpu_family, cpu_model);
            return 0;
        }
        switch (cpu_model)
        {
        /* Sandy Bridge */
        case 0x2a:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_GPU] = true;
            is_intel = true;
            break;
        case 0x2d:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        /* Ivy Bridge */
        case 0x3a:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_GPU] = true;
            is_intel = true;
            break;
        case 0x3e:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        /* Haswell */
        case 0x3c:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            supported[X86_ENERGY_COUNTER_GPU] = true;
            is_intel = true;
            break;
        case 0x45:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        case 0x46:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        case 0x3f:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        /* Broadwell*/
        case 0x3d:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            supported[X86_ENERGY_COUNTER_GPU] = true;
            is_intel = true;
            break;
        case 0x47:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        case 0x56:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        case 0x4f:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        /* Skylake*/
        case 0x4e:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            is_intel = true;
            break;
        case 0x55:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        case 0x5e:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
            break;
        /* Alderlake */
        case 0x97:
        case 0x9a:
        case 0xbf:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_GPU] = true;
            is_intel = true;
            break;
        case 0x8e:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_CORES] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            supported[X86_ENERGY_COUNTER_GPU] = true;
            supported[X86_ENERGY_COUNTER_PLATFORM] = true;
            break;
        case 0x8f:
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_DRAM] = true;
            is_intel = true;
        /* none of the above */
        default:
        	X86_ENERGY_SET_ERROR("Not a recognized Intel processor (family 0x%x, model 0x%x)", cpu_family, cpu_model);
            break;
        }
    }
    /* currently only Fam15h */
    else if (strcmp(buffer, "AuthenticAMD") == 0)
    {
        if (cpu_family == 0x15)
        {
            is_amd = true;
            supported[X86_ENERGY_COUNTER_PCKG] = true;
        } else if ( cpu_family == 0x17 )
        {
            is_amd_rapl = true;
            supported[X86_ENERGY_COUNTER_PCKG] = true;
            supported[X86_ENERGY_COUNTER_SINGLE_CORE] = true;
        } else
        {
        	X86_ENERGY_SET_ERROR("Not a recognized AMD processor (family 0x%x, model 0x%x)", cpu_family, cpu_model);
        }
    }

    if (is_intel)
    {
        x86_energy_mechanisms_t* t = malloc(sizeof(x86_energy_mechanisms_t));
        if ( t == NULL )
        {
            X86_ENERGY_SET_ERROR("Error allocating memory");
            return NULL;
        }

        /* initialize witrh invalid */
        for (int i = 0; i < X86_ENERGY_COUNTER_SIZE; i++)
            t->source_granularities[i] = X86_ENERGY_GRANULARITY_SIZE;

        t->name = "Intel RAPL";
        for (int i = 0; i < X86_ENERGY_COUNTER_PLATFORM; i++)
            if (supported[i])
                t->source_granularities[i] = X86_ENERGY_GRANULARITY_SOCKET;
        if (supported[X86_ENERGY_COUNTER_PLATFORM])
            t->source_granularities[X86_ENERGY_COUNTER_PLATFORM] = X86_ENERGY_GRANULARITY_SYSTEM;

        if (supported[X86_ENERGY_COUNTER_SINGLE_CORE])
            t->source_granularities[X86_ENERGY_COUNTER_SINGLE_CORE] = X86_ENERGY_GRANULARITY_CORE;

        t->nr_avail_sources = 0;
        if ( is_selected_source ( sysfs_source ) )
        {
            t->nr_avail_sources += 1;
        }
        if ( is_selected_source ( perf_source ) )
        {
            t->nr_avail_sources += 1;
        }
        if ( is_selected_source ( msr_source ) )
        {
            t->nr_avail_sources += 1;
        }
#ifdef USELIKWID
        if ( is_selected_source ( likwid_source ) )
        {
            t->nr_avail_sources += 1;
        }
#endif
#ifdef USEX86_ADAPT
        if ( is_selected_source ( x86a_source ) )
        {
            t->nr_avail_sources += 1;
        }
#endif
        if ( t->nr_avail_sources == 0 )
        {
            X86_ENERGY_SET_ERROR("No available source selected");
            free( t );
            return NULL;
        }
        t->avail_sources = malloc(t->nr_avail_sources * sizeof(x86_energy_access_source_t));
        if ( t->avail_sources == NULL )
        {
            X86_ENERGY_SET_ERROR("Error allocating memory");
            free( t );
            return NULL;
        }

        int current = 0;
        if ( is_selected_source ( sysfs_source ) )
        {
            t->avail_sources[current++] = sysfs_source;
        }
        if ( is_selected_source ( perf_source ) )
        {
            t->avail_sources[current++] = perf_source;
        }
        if ( is_selected_source ( msr_source ) )
        {
            t->avail_sources[current++] = msr_source;
        }
#ifdef USELIKWID
        if ( is_selected_source ( likwid_source ) )
        {
            t->avail_sources[current++] = likwid_source;
        }
#endif
#ifdef USEX86_ADAPT
        if ( is_selected_source ( x86a_source ) )
        {
            t->avail_sources[current++] = x86a_source;
        }
#endif
        return t;
    }
    if (is_amd)
    {
        x86_energy_mechanisms_t* t = malloc(sizeof(x86_energy_mechanisms_t));
        if ( t == NULL )
        {
            X86_ENERGY_SET_ERROR("Error allocating memory");
            return NULL;
        }
        t->name = "AMD APM";
        for (int i = 0; i < X86_ENERGY_COUNTER_SIZE; i++)
            if (supported[i])
                t->source_granularities[i] = X86_ENERGY_GRANULARITY_SOCKET;
            else
                t->source_granularities[i] = X86_ENERGY_GRANULARITY_SIZE;

        if ( ! is_selected_source ( sysfs_fam15_source ) )
        {
            X86_ENERGY_SET_ERROR("No available source selected");
            free( t );
            return NULL;
        }

        t->nr_avail_sources = 1;
        t->avail_sources = malloc(1 * sizeof(x86_energy_access_source_t));
        if ( t->avail_sources == NULL )
        {
            X86_ENERGY_SET_ERROR("Error allocating memory");
            free( t );
            return NULL;
        }
        t->avail_sources[0] = sysfs_fam15_source;

// TODO msr
#ifdef USEX86_ADAPT
// TODO x86a
#endif
        return t;
    }
    if (is_amd_rapl)
    {
        x86_energy_mechanisms_t* t = malloc(sizeof(x86_energy_mechanisms_t));
        if ( t == NULL )
        {
            X86_ENERGY_SET_ERROR("Error allocating memory");
            return NULL;
        }
        t->name = "AMD RAPL";
        for (int i = 0; i < X86_ENERGY_COUNTER_SIZE; i++)
            t->source_granularities[i] = X86_ENERGY_GRANULARITY_SIZE;

        t->source_granularities[X86_ENERGY_COUNTER_SINGLE_CORE] = X86_ENERGY_GRANULARITY_CORE;
        t->source_granularities[X86_ENERGY_COUNTER_PCKG] = X86_ENERGY_GRANULARITY_SOCKET;

        t->nr_avail_sources = 0;
        if ( is_selected_source ( msr_fam23_source ) )
        {
            t->nr_avail_sources += 1;
        }
#ifdef USEX86_ADAPT
        if ( is_selected_source ( x86a_fam23_source ) )
        {
            t->nr_avail_sources += 1;
        }
#endif
        if ( t->nr_avail_sources == 0 )
        {
            X86_ENERGY_SET_ERROR("No available source selected");
            free( t );
            return NULL;
        }
        t->avail_sources = malloc(t->nr_avail_sources * sizeof(x86_energy_access_source_t));
        if ( t->avail_sources == NULL )
        {
            X86_ENERGY_SET_ERROR("Error allocating memory");
            free( t );
            return NULL;
        }
	int current_entry=0;

        if ( is_selected_source ( msr_fam23_source ) )
        {
            t->avail_sources[current_entry++] = msr_fam23_source;
        }

#ifdef USEX86_ADAPT
        if ( is_selected_source ( x86a_fam23_source ) )
        {
            t->avail_sources[current_entry++] = x86a_fam23_source;
        }
#endif
        return t;
    }

    X86_ENERGY_SET_ERROR("could not determine capabilities of cpu, it is not supported. cpu family 0x%x, cpu model 0x%x", cpu_family, cpu_model);
    return NULL;
}

static x86_energy_architecture_node_t*
find_node_internal(x86_energy_architecture_node_t* current,
                   enum x86_energy_granularity given_granularity, unsigned long int id)
{
    if (current->granularity == given_granularity && current->id == id)
        return current;
    for (int i = 0; i < current->nr_children; i++)
    {
        x86_energy_architecture_node_t* found =
            find_node_internal(&(current->children[i]), given_granularity, id);
        if (found != NULL)
            return found;
    }

    X86_ENERGY_SET_ERROR("Could not find any children of node %s having a granularity of %d", current->name, given_granularity);
    return NULL;
}

long get_test_cpu(enum x86_energy_granularity given_granularity, unsigned long int id)
{
    x86_energy_architecture_node_t* current = arch;

    x86_energy_architecture_node_t* sub_node = find_node_internal(current, given_granularity, id);

    if (sub_node == NULL)
    {
    	X86_ENERGY_APPEND_ERROR("search for node with granularity value %d returned NULL", given_granularity);
        return -1;
    }

    while (sub_node->granularity != X86_ENERGY_GRANULARITY_THREAD)
        sub_node = sub_node->children;
    return sub_node->id;
}
