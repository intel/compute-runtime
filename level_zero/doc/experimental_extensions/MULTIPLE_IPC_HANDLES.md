<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Multiple IPC handles

* [Overview](#Overview)
* [Definitions](#Definitions)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

When using multi-tile architectures, Level Zero runtime allocations (i.e. `zeMemAllocDevice`) may be split by the GPU driver into several smaller allocations, each one allocated in a given tile. This is done for instance when using implicit scaling, to equally distribute memory on the available tiles. Although this is done as an internal optimization the application should not be aware of, there are scenarios where the number of internal allocations need to be known by the application. One of those being when exchanging the IPC handles of the runtime allocation with other process. If the allocation has been split into several smaller ones, then each of these internal allocations would have its own file descriptor, and each of them need to be separately interchanged with the other process.

Current IPC memory interfaces in the Level Zero specification `zeMemGetIpcHandle` and `zeMemOpenIpcHandle` assume the existence of only one file descriptor, and cannot be used when multiple internal allocations and file descriptors are associated with a single Level Zero allocation.

This extension provides two new interfaces with which applications can query the number of file descriptors associated with a Level Zero allocation, get those descriptors, and then create a new allocation composed of the exported internal allocations.

# Definitions

```cpp
///////////////////////////////////////////////////////////////////////////////
#ifndef ZEX_MEM_IPC_HANDLES_NAME
/// @brief Multiple IPC handles driver extension name
#define ZEX_MEM_IPC_HANDLES_NAME  "ZEX_mem_ipc_handles"
#endif // ZEX_MEM_IPC_HANDLES_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Multiple IPC handles driver extension Version(s)
typedef enum _zex_mem_ipc_handles_version_t
{
    ZEX_MEM_IPC_HANDLES_VERSION_1_0 = ZE_MAKE_VERSION( 1, 0 ),  ///< version 1.0
    ZEX_MEM_IPC_HANDLES_VERSION_CURRENT = ZE_MAKE_VERSION( 1, 0 ),  ///< latest known version
    ZEX_MEM_IPC_HANDLES_VERSION_FORCE_UINT32 = 0x7fffffff

} zex_mem_ipc_handles_version_t;
```

## Interfaces

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Returns an array IPC memory handles for the specified allocation
///
/// @details
///     - Takes a pointer to a device memory allocation and returns an array of
//        IPC memory handle for exporting it for use in another process.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + ptr` not known
ZE_APIEXPORT ze_result_t ZE_APICALL
zexMemGetIpcHandles(
    ze_context_handle_t hContext,                   ///< [in] handle of the context object
    const void *ptr,                                ///< [in] pointer to the device memory allocation
    uint32_t *numIpcHandles,                        ///< [in,out] number of IPC handles associated with the allocation
                                                    ///< if numIpcHandles is zero, then the driver shall update the value with the
                                                    ///< total number of IPC handles associated with the allocation.
    ze_ipc_mem_handle_t *pIpcHandles                ///< [in,out][optional][range(0, *numIpcHandles)] returned array of IPC memory handles
    );
```

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Creates an allocation associated with an array of IPC memory handles
///        imported from another process.
///
/// @details
///     - Takes an array of IPC memory handles from a remote process and associates it
///       with a device pointer usable in this process.
///     - The device pointer in this process should not be freed with
///       ::zeMemFree, but rather with ::zeMemCloseIpcHandle.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + handles not known
ZE_APIEXPORT ze_result_t ZE_APICALL
zexMemOpenIpcHandles(
    ze_context_handle_t hContext,                   ///< [in] handle of the context object
    ze_device_handle_t hDevice,                     ///< [in] handle of the device to associate with the IPC memory handle
    uint32_t numIpcHandles,                         ///< [in] number of IPC handles associated with the allocation
    ze_ipc_mem_handle_t *pIpcHandles,               ///< [in][range(0, *numIpcHandles)] array of IPC memory handles
    ze_ipc_memory_flags_t flags,                    ///< [in] flags controlling the operation.
                                                    ///< must be 0 (default) or a valid combination of ::ze_ipc_memory_flag_t.
    void **pptr                                     ///< [out] pointer to device allocation in this process
    );
```

## Enums

None.

# Known Issues and Limitations

* Currently supported only on Linux.
* Mainly implemented for and validated on Xe HPC (PVC) platforms.
