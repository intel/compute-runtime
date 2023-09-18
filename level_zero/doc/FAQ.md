<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Frequently asked questions (Level Zero)

For general questions,
see the [main FAQ](https://github.com/intel/compute-runtime/blob/master/FAQ.md).

## Feature: l0_cache

Mechanism to cache binary representations of GPU kernels passed to zeModuleCreate(),
to avoid compilation overheads in subsequent calls. Please see more information in
https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md#feature-cl_cache.

### Linux

#### Official instructions

##### Environment flags

NEO_CACHE_PERSISTENT - integer value to enable (1)/disable (0) on-disk binary cache. When enabled 
                       Neo will try to cache and reuse compiled binaries. Default is on.

NEO_CACHE_DIR        - path to persistent cache directory. Default values are $XDG_CACHE_HOME/neo_compiler_cache
                       if $XDG_CACHE_HOME is set, $HOME/.cache/neo_compiler_cache otherwise. If none of environment
                       variables are set then on-disk cache is disabled.

NEO_CACHE_MAX_SIZE   - Cache eviction is triggered once total size of cached binaries exceeds the value in
                       bytes (default is 1GB). Set to 0 to disable size-based cache eviction.

#### Legacy approach

In the working directory, manually create *l0_cache* directory.
The driver will use this directory to store the binary representations of the compiled kernels.
Note: This will work on all supported OSes.

##### Configuring l0_cache location

Cached kernels can be stored in a different directory than the default one.
This is useful when the application is installed into a directory
for which the user doesn't have permissions.

Set the environment variable named `l0_cache_dir` to new location of l0_cache directory.

### Windows

#### Official instructions (implementation pending)

##### Environment flags

NEO_CACHE_PERSISTENT - integer value to enable (1)/disable (0) on-disk binary cache. When enabled 
                       Neo will try to cache and reuse compiled binaries. Default is off.

NEO_CACHE_DIR        - path to persistent cache directory. Default values are %LocalAppData%\NEO\neo_compiler_cache
                       if %LocalAppData% is found. If none of environment
                       variables are set then on-disk cache is disabled.

NEO_CACHE_MAX_SIZE   - Cache eviction is triggered once total size of cached binaries exceeds the value in
                       bytes (default is 1GB). Set to 0 to disable size-based cache eviction.
#### Legacy approach

##### Windows configuration

To set the new location of cl_cache directory - add new environment variable:
1. variable name: `l0_cache_dir`
1. variable value: <destination_directory_for_l0_cache>

#### Known limitations

Please refer to OpenCL documentation: https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md#what-are-the-known-limitations-of-cl_cache-for-windows