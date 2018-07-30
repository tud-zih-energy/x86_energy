/*
 * msr_fam15.c
 *
 *  Created on: 21.06.2018
 *      Author: rschoene
 */

#include "../../include/x86_energy.h"

static int init(void)
{
	X86_ENERGY_SET_ERROR("Not yet implemented");
	return 1;
}
static x86_energy_single_counter_t setup(enum x86_energy_counter counter, size_t index)
{
	X86_ENERGY_SET_ERROR("Not yet implemented");
    return NULL;
}
static double do_read(x86_energy_single_counter_t t)
{
	X86_ENERGY_SET_ERROR("Not yet implemented");
    return -1.0;
}
static void do_close(x86_energy_single_counter_t t)
{
}
static void fini(void){};

x86_energy_access_source_t msr_fam15_source = {.name = "msr-Fam15-n/a",
                                               .init = init,
                                               .setup = setup,
                                               .read = do_read,
                                               .close = do_close,
                                               .fini = fini };
