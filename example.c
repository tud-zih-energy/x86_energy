#include <stdio.h>
#include <assert.h>

#include "x86_energy/x86_energy.h"

int main() {
	int nr_packages, i;
	struct x86_energy_source *source = get_available_sources();
	assert(source != NULL);
	nr_packages = source->get_nr_packages();
	printf("Found %d packages.\n", nr_packages);
	
	for(i = 0; i < nr_packages; i++)
		source->init_device(i);


	while (1) {
		for (i = 0; i < nr_packages; i++)
			printf("Power for package %d: %f\n", i, source->get_power(i));
		sleep(1);
	}

	return 0;
}
