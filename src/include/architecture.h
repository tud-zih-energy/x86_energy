/*
 * architecture.h
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#ifndef SRC_INCLUDE_ARCHITECTURE_H_
#define SRC_INCLUDE_ARCHITECTURE_H_

#include "../../include/x86_energy.h"
/**
 * Get a specific CPU from a given granularity
 */
long get_test_cpu(enum x86_energy_granularity given_granularity, unsigned long int id);


#endif /* SRC_INCLUDE_ARCHITECTURE_H_ */
