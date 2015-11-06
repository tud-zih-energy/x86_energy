cmake_minimum_required(VERSION 2.8)
project(x86_energy)

option(BUILD_STATIC "Build a static x86_energy library." OFF)
option(MSR_STATIC "Link msr library static." ON)
option(MSR_DIR "Path to libmsr directory" "")
option(MSR_INC "Path to libmsr headers" "")
option(MSR_LIB "Path to libmsr" "")
option(X86_ADAPT "Enables x86_adapt support" OFF)
option(X86_ADAPT_STATIC "Link x86_adapt library static." ON)

set(X86_ADAPT_FOUND false)

set(PLUGIN_SOURCE x86_energy.c rapl.c amd_fam15h_power.c)
set(PLUGIN_HEADER x86_energy.h)


if(X86_ADAPT)
    include(common/FindX86_adapt.cmake)
    if(X86_ADAPT_FOUND)
        include_directories(${rn})
        add_definitions("-DX86_ADAPT")
    else()
        message("libx86_adapt was not found, falling back to libmsr!")
    endif()
endif()


if(NOT X86_ADAPT_FOUND OR NOT X86_ADAPT)
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
        message("Found neither libx86_adapt nor libmsr! Building libmsr.")
        set(MSR_NAME msr)
        set(MSR_SOURCE msr.c)
        if(MSR_STATIC)
            add_library(${MSR_NAME} STATIC ${MSR_SOURCE})
        else()
            add_library(${MSR_NAME} SHARED ${MSR_SOURCE})
        endif()
    endif()
endif()


#additional c flags
set(CMAKE_C_FLAGS "-fPIC")
#debugging c flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")
#release c flags
set(CMAKE_C_FLAGS_RELEASE "-Os")

if(BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC ${PLUGIN_SOURCE})
else()
    add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCE})
endif()

if(X86_ADAPT_FOUND)
    target_link_libraries(${PROJECT_NAME} ${X86_ADAPT_LIBRARIES})
else()
    if(NOT USED_LIB_NAME)
        target_link_libraries(${PROJECT_NAME} ${MSR_NAME})
    endif()
endif()
target_link_libraries(${PROJECT_NAME} pthread m)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(FILES ${PLUGIN_HEADER} DESTINATION include)
