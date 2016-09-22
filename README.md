[![Build Status](https://travis-ci.org/tud-zih-energy/x86_energy.svg?branch=master)](https://travis-ci.org/tud-zih-energy/x86_energy)

#MSR & X86-Energy Libraries for Score-P (and VampirTrace)

The `msr` library faciliates to reduce the number of open file handles to `/dev/*/msr` when multiple
plugins are used. This is done by reference counting on the file descriptors.

The `x86_energy` library allows to count power and energy values for Intel Sandybridge, AMD Bulldozer
and newer architectures. It supports reading `msr` registers directly or through the `x86_adapt`
library on Intel Sandybridge. None of them is needed for reading power and energy values on
AMD Bulldozer CPUs.

##Compilation and Installation

###Prerequisites

To compile this plugin, you need:

* GCC compiler

* `libpthread`

* CMake

* Reading `msr` directly:

    The kernel module `msr` should be active (you might use `modprobe`) and you should have reading
    access to `/dev/cpu/*/msr`.

* Reading energy values through `x86_adapt`:

    The kernel module `x86_adapt_driver` should be active and and should have reading access to
    `/dev/x86_adapt/cpu/*`.

###Build Options

* `X86_ADAPT` (default off)

    Uses `x86_adapt` library instead of accessing `msr` directly.


* `X86A_STATIC` (default on)

    Link `x86_adapt` statically.

* `X86A_DIR`

    Path to `x86_adapt` directory.
    Searches in `X86A_DIR/include` for the header files and in `X86A_DIR/lib` for the library.


* `X86A_INC`

    Path to `x86_adapt` header files.

* `X86A_LIB`

    Path to `x86_adapt` library.


* `MSR_STATIC` (default on)

    Link `libmsr` statically.

* `MSR_DIR`

    Path to `libmsr` directory.

* `MSR_INC`

    Path to `libmsr` headers.

* `MSR_LIB`

    Path to `libmsr` library.

###Building

1. Create build directory

        mkdir build
        cd build

2. Invoking CMake

        cmake ..

    Example for using the `x86_adapt` library:

        cmake .. -DX86_ADAPT=1 -DX86A_DIR=~/Software/x86_adapt

3. Invoking make

        make

4. Copy it to a location listed in `LD_LIBRARY_PATH` or add current path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

##Usage

###If anything fails

1. Check whether the libraries can be loaded from the `LD_LIBRARY_PATH`.

2. Check whether you are allowed to read `/dev/cpu/*/msr` or `/dev/x86_adapt/cpu/*`.

3. Write a mail to the author.

##Authors

* Robert Schoene (robert.schoene at tu-dresden dot de)

* Michael Werner (michael.werner3 at tu-dresden dot de)
