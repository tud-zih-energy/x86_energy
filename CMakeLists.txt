cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0048 NEW)
project(x86_energy VERSION 2.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_library(x86_energy SHARED
    src/architecture/architecture.c
    src/architecture/overflow_thread.c
    src/architecture/parse_architecture.c
    src/access/msr_fam15.c
    src/access/msr.c
    src/access/perf.c
    src/access/procfs.c
    src/access/sysfs_fam15.c
    src/access/sysfs.c
)

target_link_libraries(x86_energy PUBLIC Threads::Threads m)

find_package(X86Adapt)

if (X86Adapt_FOUND)
    target_include_directories(x86_energy PRIVATE ${X86_ADAPT_INCLUDE_DIRS})
    target_sources(x86_energy PRIVATE src/access/x86_adapt.c)
    target_compile_definitions(x86_energy PRIVATE USEX86_ADAPT)
    target_link_libraries(x86_energy PUBLIC ${X86_ADAPT_LIBRARIES})
endif()

find_package(Likwid)

if (LIKWID_FOUND)
    target_include_directories(x86_energy PRIVATE ${LIKWID_INCLUDE_DIRS})
    target_sources(x86_energy PRIVATE src/access/likwid.c)
    target_compile_definitions(x86_energy PRIVATE USELIKWID)
    target_link_libraries(x86_energy PUBLIC ${LIKWID_LIBRARIES})
endif()

target_include_directories(x86_energy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(x86_energy PROPERTIES PUBLIC_HEADER "include/x86_energy.h;include/x86_energy.hpp")
target_compile_features(x86_energy PUBLIC c_std_99)

add_library(x86_energy::x86_energy ALIAS x86_energy)

add_library(x86_energy_cxx INTERFACE)
target_link_libraries(x86_energy_cxx INTERFACE x86_energy::x86_energy)
target_compile_features(x86_energy_cxx INTERFACE cxx_std_11)

add_library(x86_energy::x86_energy_cxx ALIAS x86_energy_cxx)

add_subdirectory(test)

install(TARGETS x86_energy x86_energy_cxx
    EXPORT x86_energyTargets
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

install(EXPORT x86_energyTargets
    DESTINATION share/cmake/x86_energy
    NAMESPACE x86_energy::
)

include(CMakePackageConfigHelpers)
write_basic_package_version_File("x86_energyConfigVersion.cmake"
    VERSION ${x86_energy_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES "x86_energyConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/x86_energyConfigVersion.cmake"
    DESTINATION share/cmake/x86_energy
)
