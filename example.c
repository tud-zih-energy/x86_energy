#include <stdio.h>
#include <assert.h>

#include "x86_energy/x86_energy.h"

int main() {
	int nr_packages, i, j;
	struct x86_energy_source *source = get_available_sources();
	assert(source != NULL);
	nr_packages = source->get_nr_packages();
	printf("Found %d packages.\n", nr_packages);
	
	for(i = 0; i < nr_packages; i++)
		source->init_device(i);


	while (1) {
		for (i = 0; i < nr_packages; i++) {
            for (j = 0; j < source->plattform_features->num; j++) {
			    printf("Power for %s on package %d: %f\n", 
                        source->plattform_features->name[j],
                        i, source->get_power(i, j));
            }
        }
		sleep(1);
	}

	return 0;
}
