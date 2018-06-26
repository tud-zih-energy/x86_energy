add_executable(x86_energy_example test.c)
target_link_libraries(x86_energy_example PRIVATE x86_energy::x86_energy)

add_executable(x86_energy_example_cxx test.cpp)
target_link_libraries(x86_energy_example_cxx PRIVATE x86_energy::x86_energy_cxx)
