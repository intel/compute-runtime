
# Frequently asked questions

## OS support

### Which Linux versions does NEO support?

NEO should work on any modern Linux distribution (i.e. Ubuntu, Fedora, etc.)
with default / stock configuration (no kernel patches), assuming the underlying
kernel's drm subsystem is 4.7 or higher. Newer platforms will require a kernel version
that provides support for that platform (e.g. Coffee Lake requires kernel 4.14 or higher).

Our default (most frequent) validation config is currently (as of Q4'18) Ubuntu 18.04 LTS.

Note: Neo will not work correctly on legacy kernels with Intel patches (e.g. 4.7.0.intel.r4.0).

### Does NEO support Microsoft Windows?

Our closed-source driver for Windows is using the same codebase. At this time,
we do not support compilation of the OpenCL stack for Windows. It is our long-term
intention to offer that option.

### Why is the feature set different in latest Windows driver vs. latest NEO on github?

Our Windows release process takes up to several weeks before drivers are available through intel.com
and/or Windows update. Features available in github will be available on Windows later.

Note: Older platforms (e.g. Broadwell) are considered to be in maintenance mode for Windows.

### Does NEO support Android?

The code can be ported and cross-compiled for Android systems. It is our long-term intention to offer that option.

## Platform support

### Which Intel platforms are supported by the driver?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### How can I check that my specific device is supported by the driver?

To check support for any device, you can follow these steps:
1. Go to [Ark]( https://ark.intel.com) and find your Device ID
1. Find the corresponding device ID label in [GMM]( https://github.com/intel/gmmlib/blob/master/Source/inc/common/igfxfmid.h)
1. Check if this device ID label is enumerated in the [supported device list](https://github.com/intel/compute-runtime/blob/master/runtime/dll/linux/devices/devices_base.inl)

### When will support for platform X be added?

We will start adding platform support after platform is disclosed by Intel.
It is our intention to offer full support ahead of platform's market availability.

## OpenCL version

### Which version of OpenCL is supported?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### Which platforms will receive OpenCL 2.2 support?

Any platforms supporting OpenCL 2.1 are eligible for move to OpenCL 2.2.

### How can I enable OpenCL 2.1 on the Apollo Lake / Broxton platform?

You can enable a higher version of OpenCL using the ForceOCLVersion debug flag.

## Feature: cl_cache

### What is cl_cache?

This is a mechanism to cache binary representations of OpenCL kernels provided in text form by the application.
By storing the binary representations, compiling is required only the first time, which improves performance.

### How can cl_cache be enabled?

In the working directory, manually create *cl_cache* directory.
The driver will use this directory to store the binary representations of the compiled kernels.
Note: This will work on all supported OSes.

### Configuring cl_cache location

Cached kernels can be stored in a different directory than the default one. This is useful when the application is installed into a directory for which the user doesn't have permissions.

#### Linux configuration

Set the environment variable named `cl_cache_dir` to new location of cl_cache directory.

#### Example:

If the application's directory is `/home/user/Document`, by default cl_cache will be stored in `/home/user/Document/cl_cache`.
If the new path should be `/home/user/Desktop/cl_cache_place`, set environment variable `cl_cache_dir` to `/home/user/Desktop/cl_cache_place`.
```bash
export cl_cache_dir=/home/user/Desktop/cl_cache_place
```

Subsequent application runs with passed source code and `cl_cache_dir` environment variable set will reuse previously cached kernel binaries instead of compiling kernels from source.

#### Windows configuration

To set the new location of cl_cache directory - in the registry `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL`:
1. add key `cl_cache_dir`
1. add string value named <path_to_app> to `cl_cache_dir` key
1. set data of added value to desired location of cl_cache

#### Example:

If application is located in `C:\Program Files\application\app.exe`, by default cl_cache will be stored in `C:\Program Files\application\cl_cache`.
If the new path should be `C:\Users\USER\Documents\application\cl_cache`, to subkey `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL\cl_cache_dir` add string value named `C:\Program Files\application\app.exe` with data `C:\Users\USER\Documents\application\cl_cache`.

e.g.
string value : `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL\cl_cache_dir\C:\Program Files\application\app.exe`
data : `C:\Users\USER\Documents\application\cl_cache`

Neo will look for string value (REG_SZ) `C:\Program Files\application\app.exe` in key `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL\cl_cache_dir`. Data of this string value will be used as new cl_cache dump directory for this specific application.

### What are the known limitations of cl_cache?

1. Not thread safe. (Workaround: Make sure your clBuildProgram calls are executed in thread safe fashion.)
1. Binary representation may not be compatible between various versions of NEO and IGC drivers. (Workaround: Manually empty *cl_cache* directory prior to update)
1. Cache is not automatically cleaned. (Workaround: Manually empty *cl_cache* directory)
1. Cache may exhaust disk space and cause further failures.  (Workaround: Monitor and manually empty *cl_cache* directory)
1. Cache is not process safe.

## Feature: Out of order queues

### Implementation details of out of order queues implementation

Current implementation of out of order queues allows multiple kernels to be run concurently. This allows for better device utilization in scenarios where single kernel
doesn't fill whole device.

More details can be found here:

https://github.com/intel/compute-samples/tree/master/compute_samples/applications/commands_aggregation

https://www.iwocl.org/wp-content/uploads/iwocl-2019-michal-mrozek-intel-breaking-the-last-line-of-performance-border.pdf

### Known issues and limitations

1. Turning on profiling on out of order command queue serializes kernel execution.
1. Blocking command queue with user events blocks all further submissions until event is unblocked.
1. Commands blocked by user events, when unblocked are serialized as well.

## Who are we?

NEO OpenCL team is part of VTT (Visual Technologies Team). 
Historically, our team was responsible for delivery of the closed source OpenCL driver for Windows, Linux, and Android.

Most of the developers are located in Poland (UTC+1 timezone).

### I would like to join your team. How can I apply for a job?

1. Go to: https://jobs.intel.com/ListJobs/All/Search/city/Gdansk/
1. Filter for OpenCL in the job title
1. Browse the descriptions looking for positions mentioning this GitHub repository
