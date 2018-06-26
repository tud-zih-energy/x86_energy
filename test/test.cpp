#include <x86_energy.hpp>

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    x86_energy::Architecture hw_root;

    x86_energy::Mechanism a;

    std::cout << "Architecture: " << a.name() << std::endl;

    auto sources = a.available_sources();

    for (auto& source : sources)
    {
        std::cout << "Testing source " << source.name() << std::endl;

        try
        {
            source.init();
        }
        catch (std::exception& e)
        {
            std::cerr << "Init failed: " << e.what() << std::endl;
        }

        for (int j = 0; static_cast<x86_energy::Counter>(j) < x86_energy::Counter::SIZE; j++)
        {
            auto counter = static_cast<x86_energy::Counter>(j);
            auto granularity = a.granularity(counter);

            if (granularity >= x86_energy::Granularity::SIZE)
            {
                continue;
            }

            std::cout << "Try counter " << counter << std::endl;

            for (int package = 0; package < hw_root.size(granularity); package++)
            {
                std::cout << "avail for granularity " << granularity << ". There are "
                          << hw_root.size(granularity)
                          << " devices avail for this counter, testing " << package << std::endl;

                try
                {
                    auto source_counter = source.get(counter, package);

                    double value = source_counter.read();
                    std::cout << "Read value: " << value << std::endl;

                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    double value2 = source_counter.read();
                    std::cout << "Read value: " << value2 - value << std::endl;

                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    value = source_counter.read();
                    std::cout << "Read value: " << value - value2 << std::endl;

                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    value2 = source_counter.read();
                    std::cout << "Read value: " << value2 - value << std::endl;
                }
                catch (std::exception& e)
                {
                    std::cerr << "Could not use counter " << counter << " for package\n";
                }
            }
        }
    }
}
