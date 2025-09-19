<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Compiler Cache

- [Introduction](#Introduction)
- [Configuration](#Configuration)
- [Implementation](#Implementation)
- [Key Features](#Key-Features)
- [Debug Keys](#Debug-Keys)
- [Potential Issues and Limitations](#Potential-Issues-and-Limitations)

# Introduction

Compiler cache (cl_cache) is a mechanism that can be used to improve the performance of just-in-time (JIT) compilation.
When a kernel is compiled, the binary is stored in the cache. If a compilation of the same kernel is requested, the binary from the cache is used instead of being recompiled. Utilizing the cache can substitute the need for compiling kernels in the primary source format - OpenCL C as well as in an intermediate representation, such as SPIRV. Compute Runtime implements a persistent cache, which means storing compiled kernels on a long-term basis. Cached files are preserved across multiple runs of OpenCL/Level Zero applications and only deleted once eviction is needed.

The cache mechanism in Compute Runtime is supported by two APIs: OpenCL and Level Zero.

The following file extensions are used:

- OpenCL: *.cl_cache*
- Level Zero: *.l0_cache*

Each cache file has a \<hash>.\<extension> format.<br>
Hash is a unique identifier for a file, created based on the following attributes:

Kernel sources
- Translation Input

Provided options
- Build API Options
- Build Internal Options

User defined constants
- Specialization Constant Identifiers
- Specialization Constant Values

Identifying the compiler's unique version
- IGC Revision
- IGC Library Size
- IGC Library Modification Time

Additional information
- Hardware Info

The resulting hash is identical for a particular set of variables, which means that the same variables always generate the same hash.
This ensures that we always read/write the right binary file under the given conditions.

# Configuration

## Windows

| Environment Variable | Value                                                              | Description                                                                                                                                                                                    |
| -------------------- | ------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| NEO_CACHE_PERSISTENT | 0: disabled<br>1: enabled<br>Default: 1                            | Enable or disable on-disk binary cache.<br>When enabled Compute Runtime will try to cache and reuse compiled binaries.                                                                                     |
| NEO_CACHE_DIR        | \<Absolute path><br>Default: %LocalAppData%\NEO\neo_compiler_cache | Path to persistent cache directory.<br>If `NEO_CACHE_DIR` is not set and %LocalAppData% could not be accessed, <br>on-disk cache is disabled.                                                  |
| NEO_CACHE_MAX_SIZE   | \<Size in bytes><br>Default: 1 GB                                  | Maximum size of compiler cache in bytes.<br>Total size of files stored in the cache will never exceed this value.<br>If adding a new binary would cause the cache to exceed its limit, the eviction mechanism is triggered.<br>Set to 0 to disable size-based cache eviction. |

## Linux

| Environment Variable | Value                                                           | Description                                                                                                                                                                                                                                                      |
| -------------------- | --------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| NEO_CACHE_PERSISTENT | 0: disabled<br>1: enabled<br>Default: 1                         | Enable or disable on-disk binary cache.<br>When enabled Compute Runtime will try to cache and reuse compiled binaries.                                                                                                                                                       |
| NEO_CACHE_DIR        | \<Absolute path><br>Default: $XDG_CACHE_HOME/neo_compiler_cache | Path to persistent cache directory.<br>Default value is $XDG_CACHE_HOME/neo_compiler_cache if $XDG_CACHE_HOME is set, $HOME/.cache/neo_compiler_cache otherwise.<br>If neither `NEO_CACHE_DIR`, $XDG_CACHE_HOME nor $HOME is defined, on-disk cache is disabled. |
| NEO_CACHE_MAX_SIZE   | \<Size in bytes><br>Default: 1GB                                | Maximum size of compiler cache in bytes.<br>Total size of files stored in the cache will never exceed this value.<br>If adding a new binary would cause the cache to exceed its limit, the eviction mechanism is triggered.<br>Set to 0 to disable size-based cache eviction.                                                                   |

# Implementation

## Cache Creation

When persistent cache is enabled, at first occurrence driver creates *config.file* which contains the directory size and is also entry point to caching mechanism. As cache files are created/evicted, the contents of *config.file* are automatically updated.
Such mechanism prevents the total cache size from being calculated at each compilation and prevents iterating through all files each time.

Each write to disk has following steps:

1. lock *config.file* (advisor lock)
1. create temporary file
1. write contents to file
1. rename temporary file to proper hash name
1. store updated directory size in the *config.file*
1. unlock *config.file*

## Cache Eviction

Since Compute Runtime cl_cache is persistent type of cache, it is not cleared when the application has finished or the system is rebooted.
It stays until eviction is needed, which is when the cache limit set by the `NEO_CACHE_MAX_SIZE` is about to be reached.

At such point, Compute Runtime activates the eviction mechanism, which works as follows:

1. lock *config.file* (advisor lock)
1. gather all cache files
1. sort files by last access time value (read/write)
1. remove least recently accessed files with a total size of 1/3 `NEO_CACHE_MAX_SIZE`
1. store updated directory size in the *config.file*
1. unlock *config.file*

The eviction mechanism first removes the least recently accessed files, which are least likely to be reused.
This keeps the cache as up-to-date as possible.

# Key Features

- By using mutex and file locking mechanism, cl_cache provides thread and process safety
- Skipping multiple compilation results in performance gains
- Automatic eviction of old cache

# Debug Keys

The cl_cache mechanism consists of many stages and an error can occur in any of them.
For this reason, we have placed error checking at each critical point.<br>
If a problem occurred, the information about it can be read with a debug key:

`PrintDebugMessages=0/1` : when enabled, some debug messages will be propagated to console

Additionally, the `BinaryCacheTrace` flag can be used to provide deeper insights into the hash creation process:

`BinaryCacheTrace=0/1` : when enabled, cl_cache will generate a trace file containing values of all attributes used to calculate the unique hash for a compiled binary. This information can be invaluable in understanding why a specific kernel is or isn't being retrieved from the cache as expected.

*Note: `PrintDebugMessages` propagates all debug messages from the driver - it is not exclusive to cl_cache.*

# Potential Issues and Limitations

- Since the cl_cache mechanism automatically keeps track of the size of all created and deleted cache files, the user should not manually tamper with the generated files. If this were to happen, *config.file* would not have the correct directory size which could result in failure to meet the limit set by `NEO_CACHE_MAX_SIZE`. However, it is safe to manually delete the entire cache directory or all files at once.

- If we encounter issues opening or reading data from the *config.file* (e.g., due to corruption), the cache mechanism will not be utilized, and we will fall back to the standard compilation path. In this situation, information about the error will be printed in debug messages.

- cl_cache relies on the *last access time* of files to manage its eviction process effectively. If *last access time* updates are not enabled in the filesystem, cl_cache will be unable to accurately prioritize files for eviction based on their usage frequency. Consequently, files may not be evicted in the optimal order, potentially affecting cache performance. To address this limitation, future plans include implementing a backup sorting method for eviction, which may utilize file size or creation time as criteria to ensure efficient cache management even when *last access time* is unavailable.

- When generating a unique hash, Compiler Cache does not take into account the environment variables of external components. Changes in these variables may not trigger cache invalidation, which can lead to unexpected behavior and difficult to debug errors.