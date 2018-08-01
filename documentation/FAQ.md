# Frequently asked questions

## OS support

### Which Linux versions does NEO support?

NEO should work on any modern Linux distribution (i.e. Ubuntu, Fedora, etc.)
with default / stock configuration (no kernel patches), assuming the underlying
kernel's drm subsystem is 4.7 or higher. Newer platforms will require a kernel version
that provides support for that platform (e.g. Coffee Lake requires kernel 4.14 or higher).

Our default (most frequent) validation config is currently (as of Q1'18) Ubuntu 16.04.3 LTS.

### What toolchain do you recommend for building NEO on CentOS?

We recommend the following packages:
* devtoolset-4-gcc-c++
* llvm-toolset-7-clang

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

## Feature: cl_cache

### What is cl_cache?

This is a mechanism to cache binary representations of OpenCL kernels provided in text form by the application.
By storing the binary representations, compiling is required only the first time, which improves performance.

### How can cl_cache be enabled?

In the working directory, manually create *cl_cache* directory.
The driver will use this directory to store the binary representations of the compiled kernels.
Note: This will work on all supported OSes. 

### What are the known limitations of cl_cache?

1. Not thread safe. (Workaround: Make sure your clBuildProgram calls are executed in thread safe fashion.)
1. Binary representation may not be compatible between various versions of NEO and IGC drivers. (Workaround: Manually empty *cl_cache* directory prior to update)
1. Cache is not automatically cleaned. (Workaround: Manually empty *cl_cache* directory)
1. Cache may exhaust disk space and cause further failures.  (Workaround: Monitor and manually empty *cl_cache* directory)
1. Cache is not process safe.

## Who are we?

NEO OpenCL team is part of VPG (Visual and Parallel Computing group). Historically, our team
was responsible for delivery of the closed source OpenCL driver for Windows, Linux, and Android.

With that in mind, we tend to compare NEO against previous closed source releases
(i.e. [SRB5](https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver)).

Most of the developers are located in Poland (UTC+1 timezone).

### I would like to join your team. How can I apply for a job?

1. Go to: https://jobs.intel.com/ListJobs/All/Search/city/Gdansk/
1. Filter for OpenCL in the job title
1. Browse the descriptions looking for positions mentioning this GitHub repository