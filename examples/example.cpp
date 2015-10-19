#include <iostream>

extern "C" {
#include <errno.h>
#include <string.h>

#include <x86_energy.h>
}

int main()
{
    auto source = get_available_sources();
    if (!source)
    {
        std::cerr << "Failed to get x86_energy source" << std::endl;
        return -1;
    }

    const auto nr_packages = source->get_nr_packages();
    for (int package_id = 0; package_id < nr_packages; package_id++)
    {
        auto ret = source->init_device(package_id);
        if (ret)
        {
            std::cerr << "Failed to initialized x86_energy device "
                      << package_id << " of " << nr_packages << " = " << ret
                      << " (" << strerror(errno) << ")" << std::endl;
            return -1;
        }
    }
    std::cout << "Successfully initialized " << nr_packages
              << " x86_energy devices." << std::endl;

    for (int package_id = 0; package_id < nr_packages; package_id++)
    {
        std::cout << "package: " << package_id << std::endl;
        const auto nr_features = source->plattform_features->num;
        for (int feature = 0; feature < nr_features; feature++)
        {
            const auto name = source->plattform_features->name[feature];
            const auto ident = source->plattform_features->ident[feature];
            const auto energy = source->get_energy(package_id, ident);
            const auto power = source->get_power(package_id, ident);
            std::cout << name << "(" << ident << ") "
                      << "Energy: " << energy << ", Power: " << power
                      << std::endl;
        }
    }

    for (int package_id = 0; package_id < nr_packages; package_id++)
    {
        source->fini_device(package_id);
    }
    source->fini();
    source = nullptr;
}