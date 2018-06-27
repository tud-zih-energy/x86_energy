/*
 * access.h
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#ifndef SRC_INCLUDE_ACCESS_H_
#define SRC_INCLUDE_ACCESS_H_

#include "../../include/x86_energy.h"

#ifdef USELIKWID
extern x86_energy_access_source_t likwid_source;
#endif

extern x86_energy_access_source_t msr_source;
extern x86_energy_access_source_t msr_fam15_source;
extern x86_energy_access_source_t msr_fam23_source;
extern x86_energy_access_source_t perf_source;
extern x86_energy_access_source_t procfs_source;
extern x86_energy_access_source_t procfs_fam15_source;
extern x86_energy_access_source_t sysfs_source;
extern x86_energy_access_source_t sysfs_fam15_source;

#ifdef USEX86_ADAPT
extern x86_energy_access_source_t x86a_source;
extern x86_energy_access_source_t x86a_fam23_source;
#endif

#endif /* SRC_INCLUDE_ACCESS_H_ */
