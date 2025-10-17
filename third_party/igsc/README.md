# Intel(R) Graphics System Controller Firmware Update Library (IGSC FUL)
--------------------------------------------------------------------------

## Introduction
---------------

### Documentation

[API Documentation](doc/igsc_api.rst "API Documentation")

## Build

### Requirements:

#### Cross-platform

##### MeTee

  If MeTee library is not found in the system paths CMake and Meson scripts downloads
  the MeTee library sources from [GitHub](https://github.com/intel/metee)
  (git installation and correct proxy setup are required).
  Alternatively, in order to use pre-compiled MeTee one can set the following environment variables:
   * METEE_LIB_PATH to pre-compiled library path
   * METEE_HEADER_PATH to headers path

#### Linux

  * libudev (libudev-dev package in Debian)

Both CMake and meson build framework are supported.

### CMake

**Example:**

*Linux:*

```sh
    cmake -G Ninja -S . -B builddir
    ninja -v -C builddir
```

*Linux Debug version:*

```sh
    cmake -DSYSLOG:BOOL=OFF -DCMAKE_BUILD_TYPE=Debug -G Ninja -S . -B builddir
    ninja -v -C builddir
```

*Linux Debug with tests:*

```sh
    cmake -DSYSLOG:BOOL=OFF -DENABLE_TESTS:BOOL=ON -DCMAKE_BUILD_TYPE=Debug -G Ninja -S . -B builddir
    ninja -v -C builddir
```

*Windows: (Visual Studio 2019)*

From the "Developer Command Prompt for VS 2019" with CMake component installed:

```sh
    cmake -G "Visual Studio 16 2019" -S . -B builddir
    cmake --build builddir --config Release
```

*Windows Debug version: (Visual Studio 2019)*

From the "Developer Command Prompt for VS 2019" with CMake component installed:

```sh
    cmake -G "Visual Studio 16 2019" -S . -B builddir
    cmake --build builddir --config Debug
```

### meson

**Example:**

```sh
    meson setup builddir/
    meson configure -Dsyslog=true builddir
    ninja -v -C builddir/
```

## Command Line Tool Usage Example:
--------------------------

`# igsc <partition> update|version  [--image <fw image file>]  [ --device <device>]`

**Example:**

`# igsc fw version --device /dev/mei2

`# igsc oprom-data update --image <fw image file>`

## Library and CLI Version

The library is versioned according [semantic versioning 2.0.0](https://semver.org/ "semantic versioning")

*MAJOR.MINOR.PATCH-<extension>, incrementing the:

 * *MAJOR* incompatible API changes,
 * *MINOR* add functionality in a backwards compatible manner
 * *PATCH* version when you make backwards compatible bug fixes.
 * *Extension Label* git shortened commit hash or other extension.

## Artificial Intelligence
 These contents may have been developed with support from one or more Intel-operated generative artificial intelligence solutions.