<!---

Copyright (C) 2020-2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Frequently asked questions (OpenCL)

For general questions,
see the [main FAQ](https://github.com/intel/compute-runtime/blob/master/FAQ.md).

## OpenCL version

### Which version of OpenCL is supported?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

## Known Issues and Limitations

OpenCL compliance of a driver built from open-source components should not be
assumed by default. Intel will clearly designate / tag specific builds to
indicate production quality including formal compliance. Other builds should be
considered experimental.

### What is the functional delta to the "Beignet" driver?

Intel's former open-source [Beignet driver](https://github.com/intel/beignet) provided
sharing capabilities with MESA OpenGL driver. 

NEO supports platforms starting with Gen8 graphics (formerly Broadwell).
For earlier platforms, please use Beignet driver.

## Feature: cl_intel_va_api_media_sharing extension

### Where can I learn more about this extension?

See the enabling [guide](cl_intel_va_api_media_sharing.md).

## Feature: cl_cache

### What is cl_cache?

This is a mechanism to cache binary representations of OpenCL kernels provided in text form by
the application. By storing the binary representations, compiling is required only the first time,
which improves performance.

Detailed description in [programmers-guide/COMPILER_CACHE.md](https://github.com/intel/compute-runtime/blob/master/programmers-guide/COMPILER_CACHE.md)

## Feature: Out of order queues

### Implementation details of out of order queues implementation

Current implementation of out of order queues allows multiple kernels to be run concurrently.
This allows for better device utilization in scenarios where single kernel doesn't fill whole device.

More details can be found here:
* [Sample applications](https://github.com/intel/compute-samples/tree/master/compute_samples/applications/commands_aggregation)
* [IWOCL(*) presentation](https://www.iwocl.org/wp-content/uploads/iwocl-2019-michal-mrozek-intel-breaking-the-last-line-of-performance-border.pdf)

### Known issues and limitations

1. Turning on profiling on out of order command queue serializes kernel execution.
1. Blocking command queue with user events blocks all further submissions until event is unblocked.
1. Commands blocked by user events, when unblocked are serialized as well.

## Feature: Double-precision emulation (FP64)

By default NEO driver enables double precision operations only on platforms with supporting hardware.
This is signified by exposing the "cl_khr_fp64" extension in the extension string.
For other platforms, this support can be emulated by the compiler (IGC).

### How do I enable emulation?

FP64 emulation can only be enabled on Linux. There are two settings that have to be set.

#### Runtime setting:

There are two ways you can enable this feature in NEO:

* Set an environment variable **OverrideDefaultFP64Settings** to **1**:
`OverrideDefaultFP64Settings=1`

* In **igdrcl.config** configuration file in the same directory as application binary
(you may have to create this file) add a line as such:
`OverrideDefaultFP64Settings = 1`

#### Compiler setting:

IGC reads flags only from environment, so set **IGC_EnableDPEmulation** to **1** as such:
`IGC_EnableDPEmulation=1`

After both settings have been set you can run the application normally.

### Known issues and limitations

Intel does not claim full specification conformance when using emulated mode.
We reserve the right to not fix issues that appear only in emulation mode.
Performance degradation is to be expected and has not been measured by Intel.