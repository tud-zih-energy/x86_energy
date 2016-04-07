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
	execute_process(COMMAND "git" "submodule" "init")
	execute_process(COMMAND "git" "submodule" "update")
endif()

if(X86_ADAPT)
    if(X86A_STATIC)
        set(LIBX86A_NAME libx86_adapt_static.a)
    else()
        set(LIBX86A_NAME libx86_adapt.so)
    endif()
    include(common/FindX86Adapt.cmake)
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
    include(common/FindMSR.cmake)
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
set(CMAKE_C_FLAGS "-fPIC")
#debugging c flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")
#release c flags
set(CMAKE_C_FLAGS_RELEASE "-O2")

add_library(${PROJECT_NAME}_static STATIC ${PLUGIN_SOURCE})
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCE})

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})

install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_static DESTINATION lib)
install(FILES ${PLUGIN_HEADER} DESTINATION include)
