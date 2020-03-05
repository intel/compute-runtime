
# Frequently asked questions

For OpenCL specific questions,
see the [OpenCL FAQ](https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md).

## OS support

### Which Linux versions does NEO support?

NEO should work on any modern Linux distribution (i.e. Ubuntu, Fedora, etc.)
with default / stock configuration (no kernel patches), assuming the underlying
kernel's drm subsystem is 4.7 or higher. Newer platforms will require a kernel version
that provides support for that platform (e.g. Coffee Lake requires kernel 4.14 or higher).

Our default (most frequent) validation config is currently Ubuntu 18.04 LTS (as of Q1'20).

### Does NEO support Microsoft Windows?

Our closed-source driver for Windows is using the same codebase. At this time,
we do not support compilation of the stack for Windows. It is our long-term
intention to offer that option.

### Why is the feature set different in latest Windows driver vs. latest NEO on github?

Our Windows release process takes up to several weeks before drivers are available through intel.com
and/or Windows update. Features available in github will be available on Windows later.

Note: Older platforms (e.g. Broadwell) are considered to be in maintenance mode for Windows.

## Platform support

### Which Intel platforms are supported by the driver?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### How can I check that my specific device is supported by the driver?

To check support for any device, you can follow these steps:
1. Go to [Ark]( https://ark.intel.com) and find your Device ID
1. Find the corresponding device ID label in [GMM]( https://github.com/intel/gmmlib/blob/master/Source/inc/common/igfxfmid.h)
1. Check if this device ID label is enumerated in the
[supported device list](https://github.com/intel/compute-runtime/blob/master/opencl/source/dll/linux/devices/devices_base.inl)

### When will support for platform X be added?

We will start adding platform support after platform is disclosed by Intel.
It is our intention to offer full support ahead of platform's market availability.

## Who are we?

The Compute Runtime team is part of VTT (Visual Technologies Team).

Most of our engineers are located in Poland and the United States.