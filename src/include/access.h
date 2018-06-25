/*
 * access.h
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#ifndef SRC_INCLUDE_ACCESS_H_
#define SRC_INCLUDE_ACCESS_H_

#include "../../include/x86_energy.h"

extern x86_energy_access_source_t likwid_source;
extern x86_energy_access_source_t msr_source;
extern x86_energy_access_source_t msr_fam15_source;
extern x86_energy_access_source_t perf_source;
extern x86_energy_access_source_t procfs_source;
extern x86_energy_access_source_t procfs_fam15_source;
extern x86_energy_access_source_t sysfs_source;
extern x86_energy_access_source_t sysfs_fam15_source;
extern x86_energy_access_source_t x86a_source;


#endif /* SRC_INCLUDE_ACCESS_H_ */
