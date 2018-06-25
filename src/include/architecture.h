/*
 * architecture.h
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#ifndef SRC_INCLUDE_ARCHITECTURE_H_
#define SRC_INCLUDE_ARCHITECTURE_H_

/**
 * Get a specific CPU from a given package for package wide counters
 */
long get_test_cpu(unsigned long int package);

#endif /* SRC_INCLUDE_ARCHITECTURE_H_ */
