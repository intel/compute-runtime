<!---

Copyright (C) 2018-2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Frequently asked questions

For OpenCL specific questions,
see the [OpenCL FAQ](https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md).

## OS support

### Which Linux versions does NEO support?

NEO should work on any modern Linux distribution (i.e. Ubuntu, Fedora, etc.)
with default / stock configuration (no kernel patches), assuming the underlying
kernel's drm subsystem is 4.7 or higher. Newer platforms will require a kernel version
that provides support for that platform (e.g. Coffee Lake requires kernel 4.14 or higher).

Our default (most frequent) validation config is currently Ubuntu 22.04 LTS (as of Q3'22).

### Does NEO support Microsoft Windows?

Our closed-source driver for Windows is using the same codebase. At this time,
we do not support compilation of the stack for Windows. It is our long-term
intention to offer that option.

### Does NEO support Windows Subsystem for Linux (WSL)?

See [WSL.md](https://github.com/intel/compute-runtime/blob/master/WSL.md).

### Why is the feature set different in latest Windows driver vs. latest NEO on github?

Our Windows release process takes up to several weeks before drivers are available through intel.com
and/or Windows update. Features available in github will be available on Windows later.

Note: Older platforms (e.g. Broadwell) are considered to be in maintenance mode for Windows.

### How can I enable reading debug environment variables on Linux release builds?

Reading of debug environment variables on Linux release builds can be enabled by specifying
`NEOReadDebugKeys` environment variable with a value of 1.

E.g. to rebuild precompiled kernels you need to set both `RebuildPrecompiledKernels`
and `NEOReadDebugKeys` to a value of 1.

List of all debug keys can be found [here](https://github.com/intel/compute-runtime/blob/master/shared/source/debug_settings/debug_variables_base.inl).

## Platform support

### Which Intel platforms are supported by the driver?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### How can I check that my specific device is supported by the driver?

To check support for any device, you can follow these steps:
1. Go to [Ark]( https://ark.intel.com) and find your Device ID
1. Check if this Device ID is enumerated in the
[supported device list](https://github.com/intel/compute-runtime/blob/master/shared/source/dll/devices/devices_base.inl)

Note: Additional [list](https://github.com/intel/compute-runtime/blob/master/shared/source/dll/devices/devices_additional.inl) for some newer devices

### When will support for platform X be added?

We will start adding platform support after platform is disclosed by Intel.
It is our intention to offer full support ahead of platform's market availability.

## Support that will not be added to the NEO driver

There is no plan to provide the following features or support in the NEO driver (due to business reasons):
* non-Intel GPUs
* platforms older than Gen8 (Broadwell) - please use [Beignet driver](https://github.com/intel/beignet)
* sharing with OpenGL / EGL on Linux 

## Who are we?

The Compute Runtime team is part of AXG SCG (Super Compute Group).

Most of our engineers are located in Poland, United States, and India.
