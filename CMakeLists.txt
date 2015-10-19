#
cmake_minimum_required(VERSION 2.8)
project(X86_ENERGY)
#set(CMAKE_C_COMPILER clang)

# Add some flags for Debug build
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")

#does not work on ubuntu 12.04 lts
#set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "-fPIC")
set(CMAKE_C_FLAGS_RELEASE "-Os")

#Input Options
option(X86_ADAPT "Enables X86_ADAPT support" OFF)
option(X86_ADAPT_STATIC "Link x86_adapt library static." ON)

if (X86_ADAPT)
  include(common/FindX86_adapt.cmake)
endif()

#Input Options
option(MSR_STATIC "Link msr library static." ON)
option(MSR_DIR "Path to libmsr directory" "")
option(MSR_INC "Path to libmsr headers" "")
option(MSR_LIB "Path to libmsr" "")



    #static
add_library(x86_energy_static STATIC x86_energy.c rapl.c amd_fam15h_power.c)
#    set_target_properties(x86_energy_adapt_static PROPERTIES COMPILE_DEFINITIONS X86_ADAPT)
#    set_target_properties(x86_energy_adapt_static PROPERTIES INCLUDE_DIRECTORIES "${__X86_ADAPT_INC}")
target_link_libraries(x86_energy_static pthread m)

     #dyn
add_library(x86_energy SHARED x86_energy.c rapl.c amd_fam15h_power.c)
#     set_target_properties(x86_energy_adapt PROPERTIES COMPILE_DEFINITIONS X86_ADAPT)
#     set_target_properties(x86_energy_adapt PROPERTIES INCLUDE_DIRECTORIES "${__X86_ADAPT_INC}")
target_link_libraries(x86_energy pthread m)


if(X86_ADAPT_FOUND)
    include_directories(${rn})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DX86_ADAPT")
    target_link_libraries(x86_energy ${X86_ADAPT_LIBRARIES})
else()
    message(STATUS "Could NOT find libx86_adapt, trying to find libmsr")

    if(MSR_STATIC)
        set(LIBMSR_NAME libmsr_static.a)
    else()
        set(LIBMSR_NAME libmsr.so)
    endif()

    include(common/FindMSR.cmake)

    if(MSR_FOUND)
        include_directories(${MSR_INC_DIR})
        link_directories(${MSR_LIB_DIR})
        message(STATUS "Using ${MSR_LIB_DIR}")
        set(USED_LIB_NAME ${LIBMSR_NAME})
    else()
         message(STATUS "Neither libx86_adapt, nor libmsr found.")
         message(STATUS "Building libmsr.")
         if(MSR_STATIC)
            add_library(msr_static STATIC msr.c)
            target_link_libraries(x86_energy msr_static)
        else()
            add_library(msr SHARED msr.c)
            target_link_libraries(x86_energy msr)
        endif()

    endif()
endif()

install(TARGETS x86_energy x86_energy_static DESTINATION lib)
install(FILES x86_energy.h DESTINATION include)
