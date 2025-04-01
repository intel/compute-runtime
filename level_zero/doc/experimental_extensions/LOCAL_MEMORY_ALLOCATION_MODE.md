<!---

Copyright (C) 2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Local memory allocation mode

At the moment this is supported on Windows only.

In Level Zero, the API function to allocate a device (local) memory is [zeMemAllocDevice](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zememallocdevice). For such (device USM) allocations, there are two policies of handling the scenario enough resources being available:
- Local-preferred mode: A region is primarily allocated out of local-memory resources, however system-memory may be used as a fallback.
- Local-only mode: A can only be allocated out of local-memory resources. If the amount of available local-memory is not sufficient the `ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY` error is returned.

Depending on the hardware, one of these two modes is being used by default or a specific one can be enforced by setting an environment variable `NEO_LOCAL_MEMORY_ALLOCATION_MODE` to respective value:
- `NEO_LOCAL_MEMORY_ALLOCATION_MODE=0`: HW-default (value of the flag when it is not set)
- `NEO_LOCAL_MEMORY_ALLOCATION_MODE=1`: Local-only
- `NEO_LOCAL_MEMORY_ALLOCATION_MODE=2`: Local-preferred