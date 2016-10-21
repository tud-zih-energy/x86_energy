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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "x86_energy.h"

#ifdef X86_ADAPT
    #include <x86_adapt.h>
#else
    #include "msr.h"
#endif

#define MSR_RAPL_POWER_UNIT 0x606

/* enum of code names of supported intel processor architectures */
typedef enum code_names {
    UNSUPPORTED,
    SB_DESKTOP,
    SB_SERVER,
    IVY_DESKTOP,
    IVY_SERVER,
    HSW_DESKTOP,
    HSW_SERVER,
    BDW_DESKTOP,
    BDW_SERVER,
    SKL_DESKTOP,
    SKL_SERVER
} code_name;
/* code name of this system if it is supported */
static code_name this_code_name = UNSUPPORTED;

/* keep rapl_counter_ident, counter_ci_names and msr_counter_register always consistent!!!  */
enum rapl_counter_ident {
    PACKAGE = 0,
    PP0,
    PP1,
    RAM,
    RAM_0,
    RAM_1,
    RAM_2,
    RAM_3,
    NumberOfCounter
};

#ifdef X86_ADAPT
static const char* counter_ci_names[NumberOfCounter] = {
        "Intel_RAPL_Pckg_Energy",
        "Intel_RAPL_PP0_Energy",
        "Intel_RAPL_PP1_Energy",
        "Intel_RAPL_RAM_Energy",
        "Intel_DRAM_ENERGY_STATUS_CH0",
        "Intel_DRAM_ENERGY_STATUS_CH1",
        "Intel_DRAM_ENERGY_STATUS_CH2",
        "Intel_DRAM_ENERGY_STATUS_CH3"
};
#else /* Now libmsr */
static const uint64_t msr_counter_register[] = {
        0x611,
        0x639,
        0x641,
        0x619,
        0,
        0,
        0,
        0
};
#endif /* x86_adapt /libmsr */

/* list of all public counter identifier names */
static const char *rapl_domains[NumberOfCounter] = {
        "package",
        "core",
        "gpu",
        "dram",
        "dram_ch0",
        "dram_ch1",
        "dram_ch2",
        "dram_ch3"
};

static int new_joule_modifier = 0;

/* this structure maps the public counter identifier to the internal counter register */
struct ident_register_mapping {
#ifdef X86_ADAPT
    int device_type; /* X86_ADAPT_CPU or X86_ADAPT_DIE */
    int counter_register;
#else
    uint64_t counter_register;
#endif
};
static struct ident_register_mapping *ident_reg_map;

/* struct which lists the public available counter names and identifiers */
static struct plattform_features rapl_features;

/* only opens a file descriptor for the X86_ADAPT_DIE if at least one DIE feature is available on this plattform */
#ifdef X86_ADAPT
static int uncore_registers_available = 0;
#endif

struct rapl_setting {
    /* mutex to prevent race conditions while reading and calculating the counter value */
    pthread_mutex_t mutex;
    /* timestamp of the last measurement for calculating the power */
    uint64_t measure_time;
    /* number of how many overflows have been occoured */
    uint64_t overflow;
    /* value of the last reading operation to check if an overflow occured */
    uint64_t overflow_value;
    /* timestamp of the last time it was checked if an overflow occured */
    uint64_t overflow_time;
    /* value of the last reading operation with overflow bits for power value calculation */
    double   last_value;
    /* flag if the power related attributes have been intialized */
    int      power_init;
    /* flag if the overflow relatead attributes have been initilized and if the watchdog thread should monitor the counter */
    int      overflow_enabled;
    /* joule modifier is a constant which is multiplied with the raw counter value to geht the counter value in joule */
    double joule_modifier;
}__attribute__((aligned(64)));


/**
 * this structure will be allocated per intel prozessor die
 * each attribute has an array for every possible counter (except for the x86 adapt file descriptors)
 */
struct per_die_rapl {
    struct rapl_setting rapl[NumberOfCounter];
#ifdef X86_ADAPT
    /* file descriptors of the x86 adapt interface */
    int fd_cpu;
    int fd_die;
#else
    struct msr_handle msr_pckg[NumberOfCounter];
#endif
}__attribute__((aligned(64)));
static struct per_die_rapl * rapl_handles;


/* the joule modifier is a constant, which is multiplied with the counter value to get actually result in joule
 * this modifier is not the same on every platform */
static double joule_modifier_general;

/* number fo packages found on this system */
static int nr_packages = 0;


/* watchdog thread variables */
#define SECONDS55 55000000
#define SECONDS50 50000000
static pthread_t thread;
static int thread_enabled = 0;
static pthread_mutex_t thread_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_wake_up = PTHREAD_COND_INITIALIZER;


/**
 * Checks whether the current cpu is sandy bridge or newer.
 */
static code_name get_code_name(unsigned int family, unsigned int model) {
    if (family != 6)
        return UNSUPPORTED;

    switch (model) {
        case 0x2A:
            return SB_DESKTOP;
        case 0x2D:
            return SB_SERVER;
        case 0x3A:
            return IVY_DESKTOP;
        case 0x3E:
            return IVY_SERVER;
        case 0x3C:
        case 0x45:
        case 0x46:
            return HSW_DESKTOP;
        case 0x3F:
            return HSW_SERVER;
        case 0x3D:
        case 0x47:
            return BDW_DESKTOP;
        case 0x4F:
        case 0x56:
            return BDW_SERVER;
        case 0x4E:
        case 0x5E:
            return SKL_DESKTOP;
    }
    return UNSUPPORTED;
}

/**
 * Checks which plattform specific features are avaible.
 */
static inline int has_feature(int feature) {
    switch(this_code_name) {
        case SB_DESKTOP:
        case IVY_DESKTOP:
            switch(feature) {
                case PACKAGE: return 1;
                case PP0:     return 1;
                case PP1:     return 1;
                case RAM:     return 0;
                case RAM_0:   return 0;
                case RAM_1:   return 0;
                case RAM_2:   return 0;
                case RAM_3:   return 0;
            };
        case SB_SERVER:
        case IVY_SERVER:
        case HSW_SERVER:
        case BDW_SERVER:
            switch(feature) {
                case PACKAGE: return 1;
                case PP0:     return 1;
                case PP1:     return 0;
                case RAM:     return 1;
                case RAM_0:   return 1;
                case RAM_1:   return 1;
                case RAM_2:   return 1;
                case RAM_3:   return 1;
            };
        case HSW_DESKTOP:
        case BDW_DESKTOP:
        case SKL_DESKTOP:
            switch(feature) {
                  case PACKAGE: return 1;
                  case PP0:     return 1;
                  case PP1:     return 1;
                  case RAM:     return 1;
                  case RAM_0:   return 0;
                  case RAM_1:   return 0;
                  case RAM_2:   return 0;
                  case RAM_3:   return 0;
          };
        default:
            return 0;
    };
    return 0;
}

#define INTEL_EBX 0x756e6547
#define INTEL_EDX 0x49656e69
#define INTEL_ECX 0x6c65746e

/**
 * Checks if the current cpu is supported.
 */
static int check_cpuid(void) {
    unsigned int ebx, ecx, edx, max_level;
    unsigned int fms, family, model;

    ebx = ecx = edx = 0;

    asm("cpuid" : "=a" (max_level), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (0));

    if (!(ebx == INTEL_EBX && edx == INTEL_EDX && ecx == INTEL_ECX)) {
        /* not genuine intel */
        fprintf(stderr,
                "X86_ENERGY: Your processor is not Intel and therefore not supported\n");
        return -ENODEV;
    }
    asm("cpuid" : "=a" (fms), "=c" (ecx), "=d" (edx) : "a" (1) : "ebx");
    family = (fms >> 8) & 0xf;
    model = (fms >> 4) & 0xf;
    if (family == 6 || family == 0xf)
        model += ((fms >> 16) & 0xf) << 4;
    this_code_name = get_code_name(family, model);
    if (this_code_name == UNSUPPORTED) {
        fprintf(stderr, "X86_ENERGY: Not Supported Cpu Architecture, found family: %x model: %x\n", family, model);
        return -ENODEV;
    }
    return 0;
}

/**
 * Checks for an overflow and increments the counter if necessary.
 */
static inline void handle_overflow(int package_nr, int ident) {
    struct per_die_rapl *handle = &rapl_handles[package_nr];
    struct rapl_setting *rapl = &(handle->rapl[ident]);
    uint64_t data = 0;

    if (!rapl->overflow_enabled) {
        /* initialize starting values */
        rapl->overflow = 0;
        rapl->overflow_value = 0;
        rapl->overflow_enabled = 1;
    }
    rapl->overflow_time = gettime_in_us();

#ifdef X86_ADAPT

    /* read from /dev/x86_adapt/cpu */
    if (ident_reg_map[ident].device_type == X86_ADAPT_CPU) {
        x86_adapt_get_setting(handle->fd_cpu, ident_reg_map[ident].counter_register,
                              &data);
    }
    /* read from /dev/x86_adapt/node */
    else {
        x86_adapt_get_setting(handle->fd_die, ident_reg_map[ident].counter_register,
                              &data);
    }

#else
    read_msr(&handle->msr_pckg[ident]);
    data = handle->msr_pckg[ident].data;
#endif
    /* important are lower 32 bits */
    data &= 0xFFFFFFFF;

    /* Check for overflow */
    if (data < rapl->overflow_value) {
        /* Increment overflow counter */
        rapl->overflow++;
    }
    rapl->overflow_value = data;
}

static inline uint64_t __min(uint64_t oldest_time, uint64_t measure_time) {
    if (oldest_time < measure_time)
        return oldest_time;
    else
        return measure_time;
}

/**
 * The joule modifier is a constant, which is needed to convert the raw counter value into a joule value.
 * This constant is not the same on each platform and is stored in a cpu register.
 */
static inline int calculate_joule_modifier(void) {
    uint64_t modifier_u64;
    double modifier_dbl;

  /* calculate the joule modifier */
#ifdef X86_ADAPT
    int fd;
    int citem;

    fd = x86_adapt_get_device_ro(X86_ADAPT_CPU, 0);
    if (fd<0) {
        fprintf(stderr, "X86_ENERGY: failed to get adapt device (%i: %s)\n", fd, strerror(errno));
        return fd;
    }
    citem = x86_adapt_lookup_ci_name(X86_ADAPT_CPU,"Intel_RAPL_Power_Unit");
    x86_adapt_get_setting(fd, citem, &modifier_u64);
    if (citem < 0) {
        fprintf(stderr, "X86_ENERGY: Failed to get Intel_RAPL_Power_Unit\n");
        return citem;
    }
    x86_adapt_put_device(X86_ADAPT_CPU, 0);
#else
    struct msr_handle msr_rapl_power_unit;
    open_msr(0,MSR_RAPL_POWER_UNIT,&msr_rapl_power_unit);
    read_msr(&msr_rapl_power_unit);
    modifier_u64=msr_rapl_power_unit.data;
    close_msr(msr_rapl_power_unit);
#endif

    modifier_u64 &= 0x1F00;
    modifier_u64 = modifier_u64 >> 8;
    modifier_dbl = modifier_u64;
    joule_modifier_general = 1.0 / pow(2.0, modifier_dbl);

    return 0;
}

/**
 * Checks if the counter counter of the given counter should be checked if an overflow occured.
 */
static inline int is_time_to_check(int package_nr, enum rapl_counter_ident ident, uint64_t current_time) {
    uint64_t overflow_time = rapl_handles[package_nr].rapl[ident].overflow_time;
    return (current_time > overflow_time && (current_time - overflow_time) > SECONDS50) ? 1 : 0;
}

/**
 * Checks if a rapl handles is longer than 50 seconds untouched and
 * checks then if an overflow occured.
 * This thread should sleep at least for 5 seconds and can be woken up anytime by
 * sending a pthread condition signal.
 */
static void* prevent_overflow(void *ignore) {
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = 0;
    thread_enabled = 1;

    /* this one is only required for pthread_cond_timedwait */
    pthread_mutex_t thread_sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&thread_sleep_mutex);

    while(thread_enabled) {
        int i;
        int ident;
        uint64_t current_time = gettime_in_us();
        uint64_t oldest_time = current_time;
        for (i=0;i<nr_packages;i++) {
            int max_ident;
            struct per_die_rapl *handle = &rapl_handles[i];
#ifdef X86_ADAPT
            if (handle->fd_die==0) {
              max_ident = RAM_0;
            } else {
                max_ident = NumberOfCounter;
            }
#else
            max_ident = RAM_0;
#endif
            /* make sure we don't lose the handles in device_ini while in the loop */
            pthread_mutex_lock(&thread_loop_mutex);
            for (ident=0;ident<max_ident;ident++) {
                struct rapl_setting *rapl = &handle->rapl[ident];
                if (rapl->overflow_enabled) {
                    /* double checked locking to reduce locking overhead */
                    if (is_time_to_check(i, ident, current_time)) {
                        pthread_mutex_lock(&rapl->mutex);
                        if (is_time_to_check(i, ident, current_time)) {
                            handle_overflow(i, ident);
                        }
                        pthread_mutex_unlock(&rapl->mutex);
                    }
                    else {
                        oldest_time = __min(oldest_time, rapl->overflow_time);
                    }
                }
            }
            pthread_mutex_unlock(&thread_loop_mutex);
        }

        uint64_t diff = current_time - oldest_time;

        /* calcute when the thread should wake up again */
        uint64_t wakeup_at = gettime_in_us() + (SECONDS55 - diff);
        tv.tv_sec = wakeup_at/1000000;
        pthread_cond_timedwait(&thread_wake_up ,&thread_sleep_mutex, &tv);
    }

    pthread_mutex_unlock(&thread_sleep_mutex);

    return ignore;
}

/**
 * Initializes rapl plugin.
 * Checks for suitable cpu.
 * Allocates space for the handles.
 * Creates overflow prevention thread.
 */
static int rapl_init(int threaded) {
    int ret;
    int i;
    int found_features;

    if (check_cpuid())
        return check_cpuid();

    /* this sets the new joule_modifier for newer server processors */
    switch (this_code_name) {
        case HSW_SERVER:
        case BDW_SERVER:
            new_joule_modifier = 1;
            break;
        default:
            new_joule_modifier = 0;
    }

    /* initialize backends */
#ifdef X86_ADAPT
    ret = x86_adapt_init();
#else
    /* use MSR lib*/
    ret = init_msr(O_RDONLY);
#endif

    if (ret)
        return ret;

    /* count numer of features */
    for (i=0,found_features=0;i<NumberOfCounter;i++) {
        if (has_feature(i)) {
          found_features++;
        }
    }

    /* allocate space for names, idents, ident register mapping */
    rapl_features.num = found_features;
    rapl_features.name = malloc(found_features * sizeof(char *));
    if (rapl_features.name==NULL) {
        fprintf(stderr,"X86_ENERGY: Could NOT allocate memory for rapl_features.name\n");
        return -1;
    }
    rapl_features.ident = malloc(found_features * sizeof(int));
    if (rapl_features.ident==NULL) {
        fprintf(stderr,"X86_ENERGY: Could NOT allocate memory for rapl_features.ident\n");
        return -1;
    }
    ident_reg_map = malloc(found_features * sizeof(struct ident_register_mapping));
    if (ident_reg_map==NULL) {
        fprintf(stderr,"X86_ENERGY: Could NOT allocate memory for ident register map\n");
        return -1;
    }

    /* Check the avaibility of plattform specific features and add them to rapl_features.
     * x86 adapt backend:
     *  check first if it is a cpu feature otherwise check for die feature.
     *  On success add it to the identifier to register mapping.
     *  If it is neither a cpu or die feature remove it from rapl_features.
     * msr backend:
     *  add counter register to the identifier to register mapping
     * */
    for (i=0,found_features=0;i<NumberOfCounter;i++) {
        if (has_feature(i)) {
           rapl_features.name[found_features] = strdup(rapl_domains[i]);
           rapl_features.ident[found_features] = found_features;
#ifdef X86_ADAPT
            /* try to look up cpu feature */
            int citem = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, counter_ci_names[i]);
            /* it is a cpu feature */
            if (citem >= 0) {
                ident_reg_map[found_features].device_type = X86_ADAPT_CPU;
                ident_reg_map[found_features].counter_register = citem;
                found_features++;
            }
            /* check if it is a die feature */
            else {
                citem = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, counter_ci_names[i]);
                if (citem >= 0) {
                    uncore_registers_available = 1;
                    ident_reg_map[found_features].device_type = X86_ADAPT_DIE;
                    ident_reg_map[found_features].counter_register = citem;
                    found_features++;
                }
                else {
                    fprintf(stderr, "X86_ENERGY: Failed to get citem %s. Removing from rapl_features.\n",counter_ci_names[i]);
                    free(rapl_features.name[found_features]);
                    rapl_features.num--;
                }
            }
#else
            ident_reg_map[found_features].counter_register = msr_counter_register[i];
            found_features++;
#endif
        }
    }

    nr_packages = x86_energy_get_nr_packages();

    /* allocate space for the handles */
    rapl_handles=calloc(nr_packages, sizeof(struct per_die_rapl));
    if (rapl_handles==NULL) {
        fprintf(stderr,"X86_ENERGY: Could NOT allocate memory for rapl_handles\n");
        return -1;
    }

    if( (ret = calculate_joule_modifier()) )
        return ret;

    /* create thread that checks if an overflow occured */
    return (threaded) ? pthread_create(&thread, NULL, &prevent_overflow, NULL) : 0;
}

/**
 * Initializes the package.
 * Opens msr file handle.
 * Initializes rapl handles with starting values.
 */
static int rapl_init_device(int package_nr) {
    int use_cpu = -1;
    int i = 0;
    int ret = 0;
    struct per_die_rapl *handle;
    while (x86_energy_node_of_cpu(i)>=0) {
        if (x86_energy_node_of_cpu(i)==package_nr) {
            use_cpu = i;
            break;
        }
        i++;
    }

    if (use_cpu==-1) {
        fprintf(stderr,"X86_ENERGY: Could not find appropriate cpu on package %i\n",package_nr);
        exit (-1);
    }

    if (rapl_handles==NULL) {
        fprintf(stderr,"X86_ENERGY: Run init() BEFORE init_device\n");
        exit(-1);
    }

    handle = &rapl_handles[package_nr];

    /* open adapt file descriptors */
#ifdef X86_ADAPT
    handle->fd_cpu = x86_adapt_get_device_ro(X86_ADAPT_CPU, use_cpu);
    if (handle->fd_cpu > 0)
        ret = 0;
    else {
        ret = handle->fd_cpu;
        return ret;
    }
    if (uncore_registers_available) {
        handle->fd_die = x86_adapt_get_device_ro(X86_ADAPT_DIE, package_nr);
        if (handle->fd_die > 0)
            ret = 0;
        else {
            handle->fd_die = 0;
        }
    }
#else
    /* open msr handles */
    for (i=0;i<rapl_features.num;i++) {
        ret |= open_msr(use_cpu,ident_reg_map[i].counter_register,&handle->msr_pckg[i]);
    }
#endif

    /* Initialize mutexes */
    for (i=0;i<rapl_features.num;i++)
        pthread_mutex_init(&handle->rapl[i].mutex, NULL);

    /* TODO add environment variable */
    /* Initialize joule modifiers */
    for (i=0;i<rapl_features.num;i++){
      if ( new_joule_modifier && ( strstr(rapl_features.name[i],"ram") != NULL ) ) {
        /* ram channels: this is not documented, but based on observations */
        if (strstr(rapl_features.name[i],"ch") != NULL ) {
          handle->rapl[i].joule_modifier = 1.0 / pow(2.0, 18.0);
        }
        /* ram: this is kind of documented (datasheet vol. 2 for e5-1600,2600,4600 v3)*/
        else {
            handle->rapl[i].joule_modifier = 1.0 / pow(2.0, 16.0);
        }
      }
      /* default */
      else {
          handle->rapl[i].joule_modifier = joule_modifier_general;
      }
    }
    return ret;
}

/**
 * Finalizes device of given package number.
 * Disables the rapl_handle, that the overflow does not use closed msr handles.
 */
static int rapl_fini_device(int package_nr) {
    int ident;
    struct per_die_rapl *handle = &rapl_handles[package_nr];

    pthread_mutex_lock(&thread_loop_mutex);
    for (ident=0;ident<rapl_features.num;ident++) {
        handle->rapl[ident].overflow_enabled = 0;
    }
    pthread_mutex_unlock(&thread_loop_mutex);

#ifdef X86_ADAPT
    x86_adapt_put_device(X86_ADAPT_CPU, handle->fd_cpu);
    x86_adapt_put_device(X86_ADAPT_DIE, handle->fd_die);
#endif

    for (ident=0;ident<rapl_features.num;ident++) {
#ifndef X86_ADAPT
        close_msr(handle->msr_pckg[ident]);
#endif
        pthread_mutex_destroy(&handle->rapl[ident].mutex);
    }

    return 0;
}

/**
 * Gets the energy consumption for a given package number.
 */
static double __get_energy(int package_nr, int ident) {
    uint64_t current_value;
    struct rapl_setting *rapl = &(rapl_handles[package_nr].rapl[ident]);

    /* The msr register is read in handle_overflow. Therefore we don't have to read it again */
    handle_overflow(package_nr, ident);

    current_value = rapl->overflow_value | (rapl->overflow << 32);

    /* convert to joule */
    return current_value * rapl->joule_modifier;
}

/**
 * Finalizes the rapl plugin.
 * Sends a wake up signal the the overflow prevention thread to reduce the waiting time to a minimum.
 */
static int rapl_fini(void) {
    int i;
    int ret;
    thread_enabled = 0;
    pthread_cond_signal(&thread_wake_up);
    ret = pthread_join(thread, NULL);

    /* Thread is finished. Destroy corresponding mutexes and condition. */
    pthread_mutex_destroy(&thread_loop_mutex);
    pthread_cond_destroy(&thread_wake_up);

    /* free rapl features */
    for (i=0;i<rapl_features.num;i++) {
        free(rapl_features.name[i]);
        rapl_features.name[i] = NULL;
    }
    free(rapl_features.name);
    rapl_features.name = NULL;
    free(rapl_features.ident);
    rapl_features.ident = NULL;
    free(ident_reg_map);
    ident_reg_map = NULL;

#ifdef X86_ADAPT
    x86_adapt_finalize();
#endif
    return ret;
}

/**
 * Gets the energy consumption of a plattform specific counter
 * for a given package number.
 */
static double rapl_get_energy(int package_nr, int ident) {
    double dbl;

    if (ident >= NumberOfCounter)
    {
        fprintf(stderr, "Given identifier (%i) out of range (0-%i)\n", ident, NumberOfCounter);
        exit(-1);
    }

    pthread_mutex_lock(&rapl_handles[package_nr].rapl[ident].mutex);
    dbl = __get_energy(package_nr, ident);
    pthread_mutex_unlock(&rapl_handles[package_nr].rapl[ident].mutex);
    return dbl;
}

/**
 * Gets the package consumption of a platform specific counter
 * for a given package number.
 */
static double rapl_get_power(int package_nr, int ident) {
    double current_value, diff;
    double dbl = 0;
    uint64_t time_before;

    if (ident >= NumberOfCounter)
    {
        fprintf(stderr, "Given identifier (%i) out of range (0-%i)\n", ident, NumberOfCounter);
        exit(-1);
    }

    struct per_die_rapl *handle = &rapl_handles[package_nr];
    struct rapl_setting *rapl = &handle->rapl[ident];
    pthread_mutex_lock(&rapl->mutex);

    if (!rapl->power_init) {
        /* initialize starting values */
        rapl->measure_time = gettime_in_us();
        handle_overflow(package_nr, ident);
        rapl->last_value = rapl->overflow_value * rapl->joule_modifier;
        rapl->power_init = 1;
        dbl = 0;

    } else {

        time_before = rapl->measure_time;

        current_value = __get_energy(package_nr, ident);
        rapl->measure_time = gettime_in_us(); //time_after
        diff = current_value - rapl->last_value;

        rapl->last_value = current_value;
        /* divide by time */
        dbl = diff/(((rapl->measure_time-time_before))/1000000.0);
    }
    pthread_mutex_unlock(&rapl->mutex);
    return dbl;
}

static int rapl_get_nr_packages(void) {
    return nr_packages;
}

struct x86_energy_source rapl_source =
{
    GRANULARITY_DIE,
    rapl_get_nr_packages,
    rapl_init,
    rapl_init_device,
    rapl_fini_device,
    rapl_fini,
    rapl_get_power,
    rapl_get_energy,
    &rapl_features,
};
