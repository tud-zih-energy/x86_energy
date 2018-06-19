cmake_minimum_required(VERSION 2.8)
project(x86_energy)

option(X86_ADAPT "Enables x86_adapt support" OFF)
option(LIBMSR "Enables lib msr support" OFF)

if (NOT ( X86_ADAPT OR LIBMSR ) )
    message(FATAL_ERROR "Neither x86_adapt nor libmsr chosen. Please specify -DX86_ADAPT or -DLIBMSR")
endif()

if ( X86_ADAPT AND LIBMSR )
    message(FATAL_ERROR "x86_adapt and libmsr chosen. Don't know which one to use. Please specify -DX86_ADAPT or -DLIBMSR")
endif()


set(X86_ENERGY_SRC
    src/x86_energy.c
    src/rapl.c
    src/amd_fam15h_power.c)
    
set(X86_ENERGY_INC
    include/x86_energy.h)

    
add_library(x86_energy SHARED ${X86_ENERGY_SRC})
target_include_directories(x86_energy PUBLIC include)
set_target_properties(x86_energy
    PROPERTIES
        PUBLIC_HEADER ${X86_ENERGY_INC}
)
target_include_directories(x86_energy PUBLIC include)

add_library(x86_energy-static STATIC ${X86_ENERGY_SRC})
set_target_properties(x86_energy-static
    PROPERTIES
        OUTPUT_NAME x86_energy
        PUBLIC_HEADER ${X86_ENERGY_INC}
)
target_include_directories(x86_energy-static PUBLIC include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(x86_energy Threads::Threads)
target_link_libraries(x86_energy-static Threads::Threads)

# Find x86_adapt
add_library(x86_adapt-static INTERFACE IMPORTED GLOBAL)
add_library(x86_adapt INTERFACE IMPORTED GLOBAL)
find_library(X86_ADAPT_LIB_STATIC x86_adapt_static
    HINTS ENV
        LD_LIBRARY_PATH)
find_library(X86_ADAPT_LIB x86_adapt
    HINTS ENV
        LD_LIBRARY_PATH)
find_path(X86_ADAPT_INC x86_adapt.h
    HINTS  
        ENV CPATH
        ENV C_INCLUDE_PATH)
target_link_libraries(x86_adapt-static
    INTERFACE
        ${X86_ADAPT_LIB_STATIC}
    )
set_property(TARGET x86_adapt-static PROPERTY 
    INTERFACE_INCLUDE_DIRECTORIES
        ${X86_ADAPT_INC}
    )                
target_link_libraries(x86_adapt
    INTERFACE
        ${X86_ADAPT_LIB}
    )
set_property(TARGET x86_adapt PROPERTY 
    INTERFACE_INCLUDE_DIRECTORIES
        ${X86_ADAPT_INC}
    )                

# Find libmsr
add_library(libmsr-static INTERFACE IMPORTED GLOBAL)
add_library(libmsr INTERFACE IMPORTED GLOBAL)
find_library(LIBMSR_LIB_STATIC msr_static
    HINTS ENV
        LD_LIBRARY_PATH)
find_library(LIBMSR_LIB msr
    HINTS ENV
        LD_LIBRARY_PATH)
find_path(LIBMSR_INC msr.h
    HINTS  
        ENV CPATH
        ENV C_INCLUDE_PATH)
target_link_libraries(libmsr-static
    INTERFACE
        ${LIBMSR_LIB_STATIC}
    )
set_property(TARGET libmsr-static PROPERTY 
    INTERFACE_INCLUDE_DIRECTORIES
        ${LIBMSR_INC}
    )                
target_link_libraries(libmsr
    INTERFACE
        ${LIBMSR_LIB_STATIC}
    )
set_property(TARGET libmsr PROPERTY 
    INTERFACE_INCLUDE_DIRECTORIES
        ${LIBMSR_INC}
    )                

if(X86_ADAPT)
    target_link_libraries(x86_energy x86_adapt)
    target_link_libraries(x86_energy-static x86_adapt-static)
    target_compile_options(x86_energy PRIVATE "-DX86_ADAPT")
    target_compile_options(x86_energy-static PRIVATE "-DX86_ADAPT")
endif()

if(LIBMSR)
    target_link_libraries(x86_energy libmsr)
    target_link_libraries(x86_energy-static libmsr-static)
endif()

target_compile_features(x86_energy PUBLIC c_std_11)
target_compile_features(x86_energy-static PUBLIC c_std_11)
target_compile_options(x86_energy PRIVATE $<$<CONFIG:Debug>:-O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing>)
target_compile_options(x86_energy-static PRIVATE $<$<CONFIG:Debug>:-O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing>)

install(TARGETS x86_energy 
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
install(TARGETS x86_energy-static
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
