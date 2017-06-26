cmake_minimum_required(VERSION 2.8)
project(x86_energy)

option(MSR_STATIC "Link msr library static." ON)
option(X86_ADAPT "Enables x86_adapt support" OFF)
option(X86A_STATIC "Link x86_adapt library static." ON)

set(X86_ADAPT_FOUND false)

set(PLUGIN_SOURCE x86_energy.c rapl.c amd_fam15h_power.c)
set(PLUGIN_HEADER x86_energy.h)
set(LINK_LIBS pthread m)

file(GLOB FILES_IN_COMMON "common")
list(LENGTH ${FILES_IN_COMMON} COUNT_FILES_IN_COMMON)
if(${COUNT_FILES_IN_COMMON} EQUAL 0)
        message(STATUS "Initializing git submodule")
        execute_process(COMMAND "git" "submodule" "init" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
        execute_process(COMMAND "git" "submodule" "update" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()

set (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/common;${CMAKE_MODULE_PATH}")

if(X86_ADAPT)
    if(X86A_STATIC)
        set(LIBX86A_NAME libx86_adapt_static.a)
    else()
        set(LIBX86A_NAME libx86_adapt.so)
    endif()
    include(FindX86Adapt)
    if(X86_ADAPT_FOUND)
        include_directories(${X86_ADAPT_INC_DIR})
        link_directories(${X86_ADAPT_LIB_DIR})
        add_definitions("-DX86_ADAPT")
        set(LINK_LIBS ${LINK_LIBS} ${LIBX86A_NAME})
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
    include(FindMSR)
    if(MSR_FOUND)
        include_directories(${MSR_INC_DIR})
        link_directories(${MSR_LIB_DIR})
        message(STATUS "Using ${MSR_LIB_DIR}")
    else()
        message("Found neither libx86_adapt nor libmsr! Building libmsr.")
        set(MSR_SOURCE msr.c)
        if(MSR_STATIC)
            add_library(msr_static STATIC ${MSR_SOURCE})
        else()
            add_library(msr SHARED ${MSR_SOURCE})
        endif()
        link_directories(${CMAKE_CURRENT_BINARY_DIR})
    endif()
    set(LINK_LIBS ${LINK_LIBS} ${LIBMSR_NAME})
endif()


#additional c flags
set(CMAKE_C_FLAGS "-fPIC -std=gnu11")
#debugging c flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")
#release c flags
set(CMAKE_C_FLAGS_RELEASE "-O2")

#build shared library
add_library(${PROJECT_NAME}_dummy STATIC ${PLUGIN_SOURCE})
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCE})
target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})

# now some magic to merge static libraries adapted from libadapt
set(TARGET ${CMAKE_BINARY_DIR}/lib${PROJECT_NAME}_static.a)
if(X86_ADAPT)
    set(USED_SOURCE ${X86_ADAPT_LIB_DIR}/libx86_adapt_static.a)
else()
    if(MSR_FOUND)
        # libmsr is already installed on this system so we want to find it
        # we hope that also the static library was built and installled
        find_path(MSR_LIB_STATIC_DIR libmsr_static.a HINTS ${MSR_LIB} ${MSR_DIR}/lib ENV LD_LIBRARY_PATH DOC "Path to static libmsr")
    else()
        # Build static library if necessary
        if(NOT MSR_STATIC)
            add_library(msr_static STATIC ${MSR_SOURCE})
        endif()
        set(MSR_LIB_STATIC_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    set(USED_SOURCE ${MSR_LIB_STATIC_DIR}/libmsr_static.a)
endif()
set(STATIC_LIBS ${CMAKE_BINARY_DIR}/lib${PROJECT_NAME}_dummy.a ${USED_SOURCE})

add_custom_target(${PROJECT_NAME}_static ALL
    COMMAND ./merge_static_libs.sh ${TARGET} ${STATIC_LIBS}
    DEPENDS ${PROJECT_NAME}_dummy
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(FILES ${TARGET} DESTINATION lib)
install(FILES ${PLUGIN_HEADER} DESTINATION include)
