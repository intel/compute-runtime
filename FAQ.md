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

There are no plans for proactive enabling and distro adoption except for Ubuntu. We are open to accepting contributions (pull requests) that unblock support. 

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

### When will support for platform X be added?

We will start adding platform support after platform is disclosed by Intel.
It is our intention to offer full support ahead of platform's market availability.

### Why am I seeing "Resizable BAR not detected" warning and the GPU driver is not working?

If you encounter the warning:
```
WARNING: Resizable BAR not detected for device 0000:XX:00.0
```

**Note**: In older driver versions, this warning may have been displayed as `WARNING: Small BAR detected for device 0000:XX:00.0`. Both messages indicate the same underlying issue - Resizable BAR is not enabled on your system.

And observe symptoms such as:
- `clinfo` reporting `Number of platforms: 0`
- GPU not being available for compute workloads
- Driver refusing to initialize

This behavior is **expected and by design**. The GPU driver intentionally does not initialize when Resizable BAR is not detected to avoid providing a subpar user experience.

#### What is Resizable BAR?

Resizable BAR (Base Address Register) is a PCI Express feature that allows the CPU to access the entire GPU memory, rather than being limited to a small memory window (typically 256MB). Without Resizable BAR enabled:
- The driver has restricted access to GPU memory
- Performance is significantly degraded
- Some features may not function correctly
- The overall compute experience is suboptimal

#### Platform-Specific Requirements

**Platforms using the xe-kmd module:**

At this time, the GPU driver **does not support configurations without Resizable BAR** when using the xe-kmd module. The GPU driver will not function if Resizable BAR is not enabled. This is a **known limitation** for platforms using this driver stack.

To enable GPU driver support, you must enable Resizable BAR in your system BIOS/UEFI.

**Other Intel platforms:**

Enabling Resizable BAR is strongly recommended for optimal performance and full feature support. The driver intentionally prevents initialization when Resizable BAR is not detected to ensure users have the best possible experience.

#### How to Enable Resizable BAR

To resolve this issue, you need to enable Resizable BAR in your system BIOS/UEFI settings. This typically requires enabling both "Above 4G Decoding" and "Resizable BAR" (also called "Re-Size BAR Support") options.

For detailed step-by-step instructions on how to enable Resizable BAR, please refer to the following Intel support articles:
- [Resizable BAR Support for Intel Graphics](https://www.intel.com/content/www/us/en/support/articles/000090831/graphics.html)
- [Resizable BAR for Intel Arc Graphics](https://www.intel.com/content/www/us/en/support/articles/000091128/graphics/intel-arc-dedicated-graphics-family.html)
- [Resizable BAR for Intel Arc Pro Graphics](https://www.intel.com/content/www/us/en/support/articles/000099073/graphics.html)

**Note**: Some older systems or motherboards may not support Resizable BAR. Check your motherboard manufacturer's website for compatibility information and available BIOS updates.

If you continue to experience issues after enabling Resizable BAR, please consult your system or motherboard documentation, or contact your system manufacturer for assistance.

## Support that will not be added to the NEO driver

There is no plan to provide the following features or support in the NEO driver (due to business reasons):
* non-Intel GPUs
* platforms older than Gen8 (Broadwell) - please use [Beignet driver](https://github.com/intel/beignet)
* sharing with OpenGL / EGL on Linux 

## Capabilities

### How much memory is available to the application? 

Actual available memory should be read from the driver capabilities using the dedicated API. The driver will report available memory depending on the type of graphics device
(discrete vs. integrated) and operating system as follows:
* discrete on Linux - 95% of device memory
* integrated on Linux - 94% of system memory - 450 MB for internal resources
* discrete on Windows - 98% of device memory
* integrated on Windows - 94% of system memory - 450 MB for internal resources

Ignoring these instructions may lead to undefined behavior, including abort.

## Who are we?

The Compute Runtime team is part of AXG SCG (Super Compute Group).

Most of our engineers are located in Poland, United States, and India.
