
# Frequently asked questions (OpenCL)

For general questions,
see the [main FAQ](https://github.com/intel/compute-runtime/blob/master/FAQ.md).

## OpenCL version

### Which version of OpenCL is supported?

See [README.md](https://github.com/intel/compute-runtime/blob/master/README.md).

### Which platforms will receive OpenCL 2.2 support?

Any platforms supporting OpenCL 2.1 are eligible for move to OpenCL 2.2.

### How can I enable OpenCL 2.1 on the Apollo Lake / Broxton platform?

You can enable a higher version of OpenCL using the ForceOCLVersion debug flag.

## Known Issues and Limitations

OpenCL compliance of a driver built from open-source components should not be
assumed by default. Intel will clearly designate / tag specific builds to
indicate production quality including formal compliance. Other builds should be
considered experimental.

### What is the functional delta to the "Beignet" driver?

Intel's former open-source [Beignet driver](https://01.org/beignet) provided
sharing capabilities with MESA OpenGL driver. Our intention is to provide
these capabilities in NEO in the future.

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

### How can cl_cache be enabled?

In the working directory, manually create *cl_cache* directory.
The driver will use this directory to store the binary representations of the compiled kernels.
Note: This will work on all supported OSes.

### Configuring cl_cache location

Cached kernels can be stored in a different directory than the default one.
This is useful when the application is installed into a directory
for which the user doesn't have permissions.

#### Linux configuration

Set the environment variable named `cl_cache_dir` to new location of cl_cache directory.

#### Example:

If the application's directory is `/home/user/Document`, by default cl_cache will be stored in
 `/home/user/Document/cl_cache`. If the new path should be `/home/user/Desktop/cl_cache_place`,
 set environment variable `cl_cache_dir` to `/home/user/Desktop/cl_cache_place`.
```bash
export cl_cache_dir=/home/user/Desktop/cl_cache_place
```

Subsequent application runs with passed source code and `cl_cache_dir` environment variable set will
reuse previously cached kernel binaries instead of compiling kernels from source.

#### Windows configuration

To set the new location of cl_cache directory - in the registry `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL`:
1. add key `cl_cache_dir`
1. add string value named <path_to_app> to `cl_cache_dir` key
1. set data of added value to desired location of cl_cache

#### Example:

If application is located in `C:\Program Files\application\app.exe`,
by default cl_cache will be stored in `C:\Program Files\application\cl_cache`.
If the new path should be `C:\Users\USER\Documents\application\cl_cache`,
to subkey `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL\cl_cache_dir`
add string value named `C:\Program Files\application\app.exe`
with data `C:\Users\USER\Documents\application\cl_cache`.

e.g.
string value : `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL\cl_cache_dir\C:\Program Files\application\app.exe`
data : `C:\Users\USER\Documents\application\cl_cache`

Neo will look for string value (REG_SZ) `C:\Program Files\application\app.exe`
in key `HKEY_LOCAL_MACHINE\SOFTWARE\Intel\IGFX\OCL\cl_cache_dir`.
Data of this string value will be used as new cl_cache dump directory for this specific application.

### What are the known limitations of cl_cache?

1. Not thread safe.
(Workaround: Make sure your clBuildProgram calls are executed in thread safe fashion.)
1. Binary representation may not be compatible between various versions of NEO and IGC drivers.
(Workaround: Manually empty *cl_cache* directory prior to update)
1. Cache is not automatically cleaned. (Workaround: Manually empty *cl_cache* directory)
1. Cache may exhaust disk space and cause further failures.
(Workaround: Monitor and manually empty *cl_cache* directory)
1. Cache is not process safe.

## Feature: Out of order queues

### Implementation details of out of order queues implementation

Current implementation of out of order queues allows multiple kernels to be run concurently.
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