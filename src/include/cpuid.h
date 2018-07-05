/*
 * cpuid.h
 *
 *  Created on: 22.06.2018
 *      Author: rschoene
 */

#ifndef SRC_INCLUDE_CPUID_H_
#define SRC_INCLUDE_CPUID_H_

/* some definitions to parse cpuid */
#define STEPPING(eax) (eax & 0xF)
#define MODEL(eax) ((eax >> 4) & 0xF)
#define FAMILY(eax) ((eax >> 8) & 0xF)
#define TYPE(eax) ((eax >> 12) & 0x3)
#define EXT_MODEL(eax) ((eax >> 16) & 0xF)
#define EXT_FAMILY(eax) ((eax >> 20) & 0xFF)

/* cpuid call in C */
static inline void cpuid(unsigned int* eax, unsigned int* ebx, unsigned int* ecx, unsigned int* edx)
{
    /* ecx is often an input as well as an output. */
    asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "0"(*eax), "2"(*ecx));
}
#endif /* SRC_INCLUDE_CPUID_H_ */
