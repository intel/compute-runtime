<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Sysman Deviceless Initialization

* [Overview](#Overview)
* [Initialization Sequence](#Initialization-Sequence)
* [Deferred Device Discovery](#Deferred-Device-Discovery)
* [Interaction with Survivability Mode](#Interaction-with-Survivability-Mode)
* [Functional and Behavioral Considerations](#Functional-and-Behavioral-Considerations)
* [Availability](#Availability)

# Overview

A host management process may need to call `zesInit()` before any GPU device has been enumerated by the OS, for example, to read error logs that caused a failure during Driver loading and no devices are enumerated. Under the standard initialization path, `zesInit()` returns `ZE_RESULT_ERROR_UNINITIALIZED` when no GPU devices are present, preventing the management application from obtaining a valid driver handle.

To address this, an experimental init flag is introduced:

```c
#define ZES_INTEL_INIT_FLAG_EXP_NO_GPUS ZE_BIT(16)
```

When passed to `zesInit()`, this flag enables **deferred device discovery mode**: initialization succeeds even if no GPU devices are present at the time of the call. Device discovery is then deferred to the first call to `zesDeviceGet()` or a related device enumeration API if devices could not be enumerated during `zesInit()`.

# Initialization Sequence

The flag is passed as part of the `zes_init_flags_t` argument to `zesInit()`:

```c
ze_result_t result = zesInit(ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
// ZE_RESULT_SUCCESS is returned even if no GPUs are present
```

The standard GPU-only flag and the new experimental flag may be combined if needed:

```c
ze_result_t result = zesInit(ZE_INIT_FLAG_GPU_ONLY | ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
```

Any other flag combination (e.g. using bits not in `ZE_INIT_FLAG_GPU_ONLY | ZES_INTEL_INIT_FLAG_EXP_NO_GPUS`) returns `ZE_RESULT_ERROR_UNINITIALIZED` without calling the driver initialization path.

The internal initialization sequence is as follows:

1. The driver attempts normal device discovery and initialization.
2. If devices are found, a standard driver handle is created and initialization completes normally.
3. If no devices are found **and** `ZES_INTEL_INIT_FLAG_EXP_NO_GPUS` is set, a **deferred handle** is created instead. `zesInit()` returns `ZE_RESULT_SUCCESS` and `zesDriverGet()` returns a valid driver handle with `driverCount == 1`.
4. If no devices are found and the flag is **not** set, `zesInit()` returns `ZE_RESULT_ERROR_UNINITIALIZED` as usual.

# Deferred Device Discovery

When a deferred handle is active, the `ExecutionEnvironment` used during `zesInit()` is saved inside the driver handle. Device discovery runs lazily on the first call to `zesDeviceGet()`:

# Interaction with Survivability Mode

The deferred discovery path is only entered when **all** of the following are true:

1. No standard GPU devices were found during `zesInit()`.
2. No survivability-mode device handle was provided by the OS driver layer.
3. `ZES_INTEL_INIT_FLAG_EXP_NO_GPUS` is set.

If a survivability handle is present (e.g. the device is in survivability/recovery mode), that handle is used directly and deferred mode is **not** entered. The driver handle returned in that case is a normal, non-deferred handle.

# Availability

- `ZES_INTEL_INIT_FLAG_EXP_NO_GPUS` is an Intel-vendor experimental extension defined in `zes_intel_gpu_sysman.h`.
- Supported on Linux only.
- The flag occupies bit 16 of `zes_init_flags_t` to avoid conflicts with standard flags defined in bits 0 to 15.
