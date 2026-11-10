/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_MEMORY_H
#define _ZEX_MEMORY_H
#if defined(__cplusplus)
#pragma once
#endif

#include <level_zero/ze_api.h>

#include "zex_common.h"

///////////////////////////////////////////////////////////////////////////////
// It indicates that the application wants the L0 driver implementation to use
// memory referenced by **ptr passed in `zeMemAllocHost` or `zeMemAllocShared`.
// Can be set in `ze_host_mem_alloc_flags_t`.
constexpr uint32_t ZEX_HOST_MEM_ALLOC_FLAG_USE_HOST_PTR = ZE_BIT(30);

///////////////////////////////////////////////////////////////////////////////
#ifndef ZEX_MEM_IPC_HANDLES_NAME
/// @brief Multiple IPC handles driver extension name
#define ZEX_MEM_IPC_HANDLES_NAME "ZEX_mem_ipc_handles"
#endif // ZEX_MEM_IPC_HANDLES_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Multiple IPC handles driver extension Version(s)
typedef enum _zex_mem_ipc_handles_version_t {
    ZEX_MEM_IPC_HANDLES_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZEX_MEM_IPC_HANDLES_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZEX_MEM_IPC_HANDLES_VERSION_FORCE_UINT32 = 0x7fffffff

} zex_mem_ipc_handles_version_t;

#if defined(__cplusplus)
extern "C" {
#endif

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
///         + `ptr` not known
ze_result_t ZE_APICALL
zexMemGetIpcHandles(
    ze_context_handle_t hContext,    ///< [in] handle of the context object
    const void *ptr,                 ///< [in] pointer to the device memory allocation
    uint32_t *numIpcHandles,         ///< [in,out] number of IPC handles associated with the allocation
                                     ///< if numIpcHandles is zero, then the driver shall update the value with the
                                     ///< total number of IPC handles associated with the allocation.
    ze_ipc_mem_handle_t *pIpcHandles ///< [in,out][optional][range(0, *numIpcHandles)] returned array of IPC memory handles
);

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
ze_result_t ZE_APICALL
zexMemOpenIpcHandles(
    ze_context_handle_t hContext,     ///< [in] handle of the context object
    ze_device_handle_t hDevice,       ///< [in] handle of the device to associate with the IPC memory handle
    uint32_t numIpcHandles,           ///< [in] number of IPC handles associated with the allocation
    ze_ipc_mem_handle_t *pIpcHandles, ///< [in][range(0, *numIpcHandles)] array of IPC memory handles
    ze_ipc_memory_flags_t flags,      ///< [in] flags controlling the operation.
                                      ///< must be 0 (default) or a valid combination of ::ze_ipc_memory_flag_t.
    void **pptr                       ///< [out] pointer to device allocation in this process
);

#define ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE EXTENDED_ENUM(ze_result_t, 0x7fff0003)

/// @brief Map device memory allocation to host address space
///
/// @details
///    - The application may call this function from simultaneous threads.
///    - The implementation of this function should be lock-free.
///    - Host address is only valid on the host, using it on device is invalid.
///    - Freeing the memory will unmap the host memory mapping.
///    - The application must only use the memory allocation for the context
///       which was provided during allocation.
///    - The application is responsible for properly flushing/sequencing host
///       writes towards the device memory.
///    - To potentially solve ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE the user may
///       need to allocate using sharing flags by using $x_external_memory_export_desc_t.
///    - pNext points to extensible list of extensions that may be added in future (none defined yet).
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///     - ::ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
ze_result_t ZE_APICALL
zeIntelMemMapDeviceMemToHost(
    ze_context_handle_t hContext, ///< [in] handle of the context
    const void *ptr,              ///< [in] pointer to the device memory to be mapped
    void **pptr,                  ///< [out] pointer to the host mapped memory
    void *pNext                   ///< [in][optional] must be null or a pointer to an extension-specific structure
);

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_NAME
/// @brief Get handle for address range extension name
#define ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_NAME "ZE_intel_experimental_mem_get_handle_for_address_range"
#endif // ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Get handle for address range extension Version(s)
typedef enum _ze_intel_mem_get_handle_for_address_range_exp_version_t {
    ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} ze_intel_mem_get_handle_for_address_range_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Type of the handle returned by ::zeIntelMemGetHandleForAddressRange
typedef enum _ze_intel_mem_range_handle_type_exp_t {
    ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_DMA_BUF_FD = 0, ///< Linux DMA-BUF file descriptor, returned as an int
    ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_FORCE_UINT32 = 0x7fffffff
} ze_intel_mem_range_handle_type_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Location of a pointer within the object named by the returned handle
///
/// @details
///     - This structure may be passed as pNext member of ::zeIntelMemGetHandleForAddressRange.
///     - base and size describe the whole object named by the handle, which may be larger than
///       the requested range, e.g. a pool holding many allocations.
///     - The object ends size - offset bytes after ptr.
typedef struct _ze_intel_mem_range_location_exp_properties_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure
    void *pNext;                   ///< [in,out][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    uint64_t offset;               ///< [out] byte offset of ptr within the handle
    void *base;                    ///< [out] address that maps to offset 0 of the handle
    size_t size;                   ///< [out] size in bytes of the object named by the handle
} ze_intel_mem_range_location_exp_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Device support for ::zeIntelMemGetHandleForAddressRange
///
/// @details
///     - This structure may be passed as pNext member of ::ze_device_external_memory_properties_t.
typedef struct _ze_intel_device_mem_range_handle_exp_properties_t {
    ze_structure_type_ext_t stype;                    ///< [in] type of this structure
    void *pNext;                                      ///< [in,out][optional] must be null or a pointer to an extension-specific
                                                      ///< structure (i.e. contains stype and pNext).
    ze_external_memory_type_flags_t rangeExportTypes; ///< [out] handle types that can be returned for an existing allocation,
                                                      ///< 0 when the device does not support it
} ze_intel_device_mem_range_handle_exp_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Returns a handle to the memory backing an address range of an existing allocation
///
/// @details
///     - The allocation does not need to be created with an external memory export descriptor.
///     - ptr may point anywhere inside a device allocation, a pooled device allocation or a
///       virtual memory range mapped to a whole physical memory object from its start.
///     - The handle names the whole object backing ptr; chain
///       ::ze_intel_mem_range_location_exp_properties_t to locate ptr in it.
///     - When size - offset is smaller than the requested size, the handle covers only the
///       beginning of the range; call again with base + size to export the rest.
///     - Compressed memory is decompressed before the handle is returned; when that is not
///       possible the call fails with ::ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE. The whole object
///       is decompressed, so GPU work that accesses any allocation in it must be complete
///       before the call.
///     - GPU writes are visible through the handle once the host has synchronized with the GPU
///       work that made them. Writes made through the handle are visible to GPU work submitted
///       after those writes have completed.
///     - The application owns the returned handle and is responsible for closing it.
///     - The application must not free, unmap or remap the memory, or destroy its physical
///       memory, while the handle or anything created from it is in use; the driver may then
///       reuse the memory.
///     - The application may call this function from simultaneous threads.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pHandle`
///         + `nullptr == ptr`
///     - ::ZE_RESULT_ERROR_INVALID_SIZE
///         + `0 == size`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `handleType` is not a valid ::ze_intel_mem_range_handle_type_exp_t
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + `ptr` is not part of an allocation
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///         + the device does not report the handle type in
///           ::ze_intel_device_mem_range_handle_exp_properties_t
///     - ::ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE
///         + the memory is not device memory, is split across several objects, maps only part
///           of a physical memory object or cannot be decompressed
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
ze_result_t ZE_APICALL
zeIntelMemGetHandleForAddressRange(
    ze_context_handle_t hContext,                    ///< [in] handle of the context
    void *pHandle,                                   ///< [out] returned handle, an int for ::ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_DMA_BUF_FD
    const void *ptr,                                 ///< [in] start of the address range
    size_t size,                                     ///< [in] size in bytes of the address range
    ze_intel_mem_range_handle_type_exp_t handleType, ///< [in] type of the handle to return
    void *pNext                                      ///< [in,out][optional] must be null or a pointer to an extension-specific structure
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZEX_MEMORY_H
