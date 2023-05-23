<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Sysman Memory Bandwidth

* [Overview](#Overview)
* [Interfaces](#Interfaces)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

`zesMemoryGetBandwidth` exposes memory read, write counters and timestamp. The expectation is these
counters are monotonically increasing and user can get bandwidth information using two snapshots of
the read and write counters taken at different instances. However, the counters could overflow
depending on underlying width of the counter presented by the hardware, and currently there is no
mechanism with which user can detect these overflows. Therefore `zexSysmanMemoryGetBandwidth` is
introduced to resolve this problem it takes care of overflow internally. 
`zexSysmanMemoryGetBandwidth` takes duration as input argument and provides reads and write counter
values that elapsed in that duration. This interface blocks host thread progress for the duration 
specified by the user.
To calculate the bandwidth using `zexSysmanMemoryGetBandwidth` use the formula
`Bandwidth = (Readcounters + WriteCounters) / duration`

# Interfaces

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Get memory bandwidth
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hMemory`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pBandwidth`
///     - ::ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS
///         + User does not have permissions to query this telemetry.
ze_result_t ZE_APICALL
zexSysmanMemoryGetBandwidth(
    zes_mem_handle_t hMemory, ///<[in] Handleforthecomponent.
    uint64_t *pReadCounters,  ///<[out] Total bytes read from memory in duration
    uint64_t *pWriteCounters, ///<[out] Total bytes written to memory in duration
    uint64_t *pMaxBandwidth,  ///<[out] Maximum bandwidth in units of bytes/sec
    uint64_t duration         ///<[in] duration in milliseconds
);
```

```cpp
typedef ze_result_t (*pFnzexSysmanMemoryGetBandwidth)(zes_mem_handle_t, uint64 *, uint64 *, uint64 *, uint64);
...

pFnzexSysmanMemoryGetBandwidth zexSysmanMemoryGetBandwidth = nullptr;
ze_result_t res = zeDriverGetExtensionFunctionAddress(hDriver, "zexSysmanMemoryGetBandwidth", reinterpret_cast<void **>(&zexSysmanMemoryGetBandwidth)));
```

# Known Issues and Limitations

* Implemented and validation only on Xe HPC (PVC).