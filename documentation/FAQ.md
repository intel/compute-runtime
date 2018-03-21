# Frequently asked questions

## OS support

### Which Linux versions does NEO support?

NEO should work on any modern Linux distribution (i.e. Ubuntu, Fedora, etc.)
with default / stock configuration (no kernel patches), assuming the underlying
kernel's drm subsystem is 4.7 or higher. Newer platforms will require a kernel version 
that provides support for that platform (e.g. Coffee Lake requires kernel 4.14 or higher).

Our default (most frequent) validation config is currently (as of Q1'18) Ubuntu 16.04.3 LTS.

### Does NEO support Microsoft Windows?

Our closed-source driver for Windows is using the same codebase. At this time,
we do not support compilation of the OpenCL stack for Windows. It is our long-term
intention to offer that option.

### Why is the feature set different in latest Windows driver vs. latest NEO on github?

Our Windows release process takes up to several weeks before drivers are available through intel.com
and/or Windows update. Features available in github will be available on Windows later.
This is applicable for most recent driver version (currently 15.65 as of Q1'18)
- older platforms (e.g. Broadwell) are considered to be in maintenance mode for Windows.

### Does NEO support Android?

The code can be ported and cross-compiled for Android systems. It is our long-term intention to offer that option.

## Platform support

### Which Intel platforms are supported by the driver?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### When will support for platform X be added?

We will start adding platform support after platform is disclosed by Intel.
It is our intention to offer full support ahead of platform's market availability.

## OpenCL version

### Which version of OpenCL is supported?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### Which platforms will receive OpenCL 2.2 support?

Any platforms supporting OpenCL 2.1 are eligible for move to OpenCL 2.2.

## Who are we?

NEO OpenCL team is part of VPG (Visual and Parallel Computing group). Historically, our team
was responsible for delivery of the closed source OpenCL driver for Windows, Linux, and Android.

With that in mind, we tend to compare NEO against previous closed source releases
(i.e. [SRB5](https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver)).

Most of the developers are located in Poland (UTC+1 timezone).
