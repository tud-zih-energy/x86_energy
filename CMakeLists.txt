cmake_minimum_required(VERSION 2.8)
project(x86_energy)

option(GIT_UPDATE_SUBMODULES "Automatically update git submodules during CMake run" ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()


SET(SOURCES src/architecture/architecture.c src/architecture/overflow_thread.c src/architecture/parse_architecture.c src/access/msr_fam15.c  src/access/msr.c src/access/perf.c src/access/procfs.c src/access/sysfs_fam15.c src/access/sysfs.c)

find_package(X86Adapt)

if (X86Adapt_FOUND)
    include_directories(${X86_ADAPT_INCLUDE_DIRS})
    LIST(APPEND SOURCES src/access/x86_adapt.c)
    SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DUSEX86_ADAPT" )
endif()

find_package(Likwid)

if (LIKWID_FOUND)
    include_directories(${LIKWID_INCLUDE_DIRS})
    LIST(APPEND SOURCES src/access/likwid.c)
    SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DUSELIKWID" )
endif()

include_directories(include)
add_library(x86_energy SHARED ${SOURCES})
set_target_properties(x86_energy PROPERTIES PUBLIC_HEADER "include/x86_energy.h")

set_target_properties(x86_energy PROPERTIES C_STANDARD 99)

if (X86Adapt_FOUND)
    target_link_libraries(x86_energy ${X86_ADAPT_LIBRARIES})
endif()
if (LIKWID_FOUND)
    target_link_libraries(x86_energy ${LIKWID_LIBRARIES})
endif()

install(TARGETS x86_energy LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)
