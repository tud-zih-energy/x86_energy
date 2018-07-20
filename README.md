[![Build Status](https://travis-ci.org/tud-zih-energy/x86_energy.svg?branch=master)](https://travis-ci.org/tud-zih-energy/x86_energy)

# X86-Energy Libraries for Score-P

This library enables access to processor power and energy measurement facilities, i.e., RAPL and APM implementations from Intel and AMD.
It supports different backends that are checked during compile and runtime.

## Compilation and Installation

### Prerequisites

To compile this plugin, you need:

* GCC compiler

* `libpthread`

* CMake 3.9+

### Build Options

* `CMAKE_INSTALL_PREFIX` (default `/usr/local`)
    
  Installation directory
    
* `CMAKE_BUILD_TYPE` (default `Debug`)
   
   Build type with different compiler options, can be `Debug` `Release` `MinSizeRel` `RelWithDebInfo`

*  `X86_ADAPT_LIBRARIES`
    
  Libraries for x86\_adapt, e.g., `-DX86_ADAPT_LIBRARIES=/opt/x86_adapt/lib/libx86_adapt_static.a`

*  `X86_ADAPT_INCLUDE_DIRS`
    
  Include directories for x86\_adapt, e.g., `-DX86_ADAPT_INCLUDE_DIRS=/opt/x86_adapt/include`

*  `LIKWID_LIBRARIES`
    
  Libraries for likwid, e.g.`-DLIKWID_LIBRARIES=/opt/likwi/lib/liblikwid.so`

*  `LIKWID_INCLUDE_DIRS`
    
    Include directories for likwid, e.g.`-DLIKWID_INCLUDE_DIRS=/opt/likwid/include`

* `X86A_STATIC` (default on)

  Link `x86_adapt` statically, if it is found

### Building

1. Create build directory

        mkdir build
        cd build

2. Invoking CMake

        cmake .. (options)

3. Invoking make

        make
        
4. Install

        make install


5. Add the installation path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

## Usage

See the documentation in 

During runtime, the library will try to access the following interfaces:

1. intel powercap, provided by the `intel_powerclamp` kernel module
2. intel rapl via perf, provided by the `intel_rapl_perf` kernel module
3. msr and msr-safe, provided by the `msr` and `msr-safe` kernel module
4. x86-adapt, provided by the `x86_adapt` kernel module (if found during installation)
5. likwid, provided by `likwid` the `msr`/`msr-safe` kernel module (if found during installation)
6. APM fam15 APM, provided by the `fam15h_power` kernel module

Option 1-5 are provided for Intel RAPL (Intel since Sandy Bridge), Option 3 and 4 are provided for AMD RAPL (e.g., AMD Zen), option 6 is provided for APM (AMD Family 15h)

## Enforce a specific interface

You can enforce a specific interface by setting the environment variable `X86_ENERGY_SOURCE` to one of these values:

 - `likwid-rapl` selects RAPL measurement via likwid
 - `msr-rapl` selects RAPL measurement via msr/msr-safe
 - `sysfs-rapl` selects RAPL measurement via powercap-rapl sysfs entries
 - `x86a-rapl` selects RAPL measurement via x86_adapt
 - `sysfs-Fam15h` selects RAPL measurement via fam15h_power sysfs entries
 - `msr-rapl-fam23` selects AMD RAPL measurement via msr
 - `x86a-rapl-amd` selects AMD RAPL measurement via x86_adapt

### If anything fails

1. Check whether the libraries can be loaded from the `LD_LIBRARY_PATH`.

2. Write a mail to the author.

## Authors

* Robert Schoene (robert.schoene at tu-dresden dot de)

* Mario Bielert (mario.bielert@tu-dresden.de)