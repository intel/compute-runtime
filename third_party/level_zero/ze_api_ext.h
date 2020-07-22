/*
 *
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 *
 */
#ifndef _ZE_API_EXT_H
#define _ZE_API_EXT_H
#if defined(__cplusplus)
#pragma once
#endif

// standard headers
#include <level_zero/ze_api.h>

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Intel 'oneAPI' Level-Zero API common types
///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_APICALL
#if defined(_WIN32)
/// @brief Calling convention for all API functions
#define ZE_APICALL __cdecl
#else
#define ZE_APICALL
#endif // defined(_WIN32)
#endif // ZE_APICALL

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_APIEXPORT
#if defined(_WIN32)
/// @brief Microsoft-specific dllexport storage-class attribute
#define ZE_APIEXPORT __declspec(dllexport)
#else
#define ZE_APIEXPORT
#endif // defined(_WIN32)
#endif // ZE_APIEXPORT

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_DLLEXPORT
#if defined(_WIN32)
/// @brief Microsoft-specific dllexport storage-class attribute
#define ZE_DLLEXPORT __declspec(dllexport)
#endif // defined(_WIN32)
#endif // ZE_DLLEXPORT

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_DLLEXPORT
#if __GNUC__ >= 4
/// @brief GCC-specific dllexport storage-class attribute
#define ZE_DLLEXPORT __attribute__((visibility("default")))
#else
#define ZE_DLLEXPORT
#endif // __GNUC__ >= 4
#endif // ZE_DLLEXPORT

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare ze_kernel_uuid_t
typedef struct _ze_kernel_uuid_t ze_kernel_uuid_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare ze_context_desc_t
typedef struct _ze_context_desc_t ze_context_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of driver's context object
typedef struct _ze_context_handle_t *ze_context_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Supported context creation flags
typedef uint32_t ze_context_flags_t;
typedef enum _ze_context_flag_t {
    ZE_CONTEXT_FLAG_TBD = ZE_BIT(0), ///< reserved for future use
    ZE_CONTEXT_FLAG_FORCE_UINT32 = 0x7fffffff

} ze_context_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Context descriptor
typedef struct _ze_context_desc_t {
    ze_context_flags_t flags; ///< [in] creation flags.
                              ///< must be 0 (default) or a valid combination of ::ze_context_flag_t;
                              ///< default behavior may use implicit driver-based heuristics.

} ze_context_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates a context for the driver.
///
/// @details
///     - The application must only use the context for the driver which was
///       provided during creation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDriver`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == desc`
///         + `nullptr == phContext`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x1 < desc->flags`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextCreate(
    ze_driver_handle_t hDriver,    ///< [in] handle of the driver object
    const ze_context_desc_t *desc, ///< [in] pointer to context descriptor
    ze_context_handle_t *phContext ///< [out] pointer to handle of context object created
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Destroys a context.
///
/// @details
///     - The application must ensure the device is not currently referencing
///       the context before it is deleted.
///     - The implementation of this function may immediately free all Host and
///       Device allocations associated with this context.
///     - The application must **not** call this function from simultaneous
///       threads with the same context handle.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE
ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextDestroy(
    ze_context_handle_t hContext ///< [in][release] handle of context object to destroy
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Returns current status of the context.
///
/// @details
///     - The application may call this function from simultaneous threads with
///       the same context handle.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_SUCCESS
///         + Context is available for use.
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///         + Context is invalid; due to device lost or reset.
ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextGetStatus(
    ze_context_handle_t hContext ///< [in] handle of context object
);

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_MAX_KERNEL_UUID_SIZE
/// @brief Maximum kernel universal unique id (UUID) size in bytes
#define ZE_MAX_KERNEL_UUID_SIZE 16
#endif // ZE_MAX_KERNEL_UUID_SIZE

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_MAX_MODULE_UUID_SIZE
/// @brief Maximum module universal unique id (UUID) size in bytes
#define ZE_MAX_MODULE_UUID_SIZE 16
#endif // ZE_MAX_MODULE_UUID_SIZE

///////////////////////////////////////////////////////////////////////////////
/// @brief Kernel universal unique id (UUID)
typedef struct _ze_kernel_uuid_t {
    uint8_t kid[ZE_MAX_KERNEL_UUID_SIZE]; ///< [out] opaque data representing a kernel UUID
    uint8_t mid[ZE_MAX_MODULE_UUID_SIZE]; ///< [out] opaque data representing the kernel's module UUID

} ze_kernel_uuid_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Kernel properties
typedef struct _ze_kernel_propertiesExt_t {
    uint32_t numKernelArgs;        ///< [out] number of kernel arguments.
    uint32_t requiredGroupSizeX;   ///< [out] required group size in the X dimension,
                                   ///< or zero if there is no required group size
    uint32_t requiredGroupSizeY;   ///< [out] required group size in the Y dimension,
                                   ///< or zero if there is no required group size
    uint32_t requiredGroupSizeZ;   ///< [out] required group size in the Z dimension,
                                   ///< or zero if there is no required group size
    uint32_t requiredNumSubGroups; ///< [out] required number of subgroups per thread group,
                                   ///< or zero if there is no required number of subgroups
    uint32_t requiredSubgroupSize; ///< [out] required subgroup size,
                                   ///< or zero if there is no required subgroup size
    uint32_t maxSubgroupSize;      ///< [out] maximum subgroup size
    uint32_t maxNumSubgroups;      ///< [out] maximum number of subgroups per thread group
    uint32_t localMemSize;         ///< [out] local memory size used by each thread group
    uint32_t privateMemSize;       ///< [out] private memory size allocated by compiler used by each thread
    uint32_t spillMemSize;         ///< [out] spill memory size allocated by compiler
    ze_kernel_uuid_t uuid;         ///< [out] universal unique identifier.
    char name[ZE_MAX_KERNEL_NAME]; ///< [out] kernel name

} ze_kernel_propertiesExt_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieve kernel properties.
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hKernel`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pKernelProperties`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetPropertiesExt(
    ze_kernel_handle_t hKernel,                  ///< [in] handle of the kernel object
    ze_kernel_propertiesExt_t *pKernelProperties ///< [in,out] query result for kernel properties.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Dynamically link modules together that share import/export linkage
///        dependencies.
///
/// @details
///     - Modules support import and export linkage for functions and global
///       variables.
///     - Modules that have imports can be dynamically linked to export modules
///       that satisfy those import requirements.
///     - Modules can have both import and export linkages.
///     - Modules that do not have any imports or exports do not need to be
///       linked.
///     - Modules cannot be partially linked. All modules needed to satisfy all
///       import dependencies for a module must be passed in or
///       ::ZE_RESULT_ERROR_MODULE_LINK_FAILURE will returned.
///     - Modules with imports need to be linked before kernel objects can be
///       created from them.
///     - Modules will only be linked once. A module can be used in multiple
///       link calls if it has exports but it's imports will not be re-linked.
///     - Ambiguous dependencies, where multiple modules satisfy the import
///       dependencies for another module, is not allowed.
///     - ModuleGetNativeBinary can be called on any module regardless of
///       whether it is linked or not.
///     - A link log can optionally be returned to the caller. The caller is
///       responsible for destroying build log using ::zeModuleBuildLogDestroy.
///     - See SPIR-V specification for linkage details.
///     - The application may call this function from simultaneous threads as
///       long as the import modules being linked are not the same.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == phModules`
///     - ::ZE_RESULT_ERROR_MODULE_LINK_FAILURE
ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDynamicLinkExt(
    uint32_t numModules,                    ///< [in] number of modules to be linked pointed to by phModules.
    ze_module_handle_t *phModules,          ///< [in][range(0, numModules)] pointer to an array of modules to
                                            ///< dynamically link together.
    ze_module_build_log_handle_t *phLinkLog ///< [out][optional] pointer to handle of dynamic link log.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Supported command queue group property flags
typedef uint32_t ze_command_queue_group_property_flags_t;
typedef enum _ze_command_queue_group_property_flag_t {
    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE = ZE_BIT(0),             ///< Command queue group supports enqueing compute commands.
    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY = ZE_BIT(1),                ///< Command queue group supports enqueing copy commands.
    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS = ZE_BIT(2), ///< Command queue group supports cooperative kernels.
                                                                          ///< See ::zeCommandListAppendLaunchCooperativeKernel for more details.
    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS = ZE_BIT(3),             ///< Command queue groups supports metric streamers and queries.
    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_FORCE_UINT32 = 0x7fffffff

} ze_command_queue_group_property_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Command queue group properties queried using
///        ::zeDeviceGetCommandQueueGroupProperties
typedef struct _ze_command_queue_group_properties_t {
    void *pNext;                                   ///< [in,out][optional] pointer to extension-specific structure
    ze_command_queue_group_property_flags_t flags; ///< [out] 0 (none) or a valid combination of
                                                   ///< ::ze_command_queue_group_property_flag_t
    size_t maxMemoryFillPatternSize;               ///< [out] maximum `pattern_size` supported by command queue group.
                                                   ///< See ::zeCommandListAppendMemoryFill for more details.
    uint32_t numQueues;                            ///< [out] the number of physical command queues within the group.

} ze_command_queue_group_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieves command queue group properties of the device.
///
/// @details
///     - Properties are reported for each physical command queue type supported
///       by the device.
///     - Multiple calls to this function will return properties in the same
///       order.
///     - The order in which the properties are returned defines the command
///       queue group's ordinal.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @remarks
///   _Analogues_
///     - **vkGetPhysicalDeviceQueueFamilyProperties**
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pCount`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCommandQueueGroupProperties(
    ze_device_handle_t hDevice,                                       ///< [in] handle of the device
    uint32_t *pCount,                                                 ///< [in,out] pointer to the number of command queue group properties.
                                                                      ///< if count is zero, then the driver will update the value with the total
                                                                      ///< number of command queue group properties available.
                                                                      ///< if count is non-zero, then driver will only retrieve that number of
                                                                      ///< command queue group properties.
                                                                      ///< if count is larger than the number of command queue group properties
                                                                      ///< available, then the driver will update the value with the correct
                                                                      ///< number of command queue group properties available.
    ze_command_queue_group_properties_t *pCommandQueueGroupProperties ///< [in,out][optional][range(0, *pCount)] array of query results for
                                                                      ///< command queue group properties
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Appends a memory write of the device's global timestamp value into a
///        command list.
///
/// @details
///     - The application must ensure the events are accessible by the device on
///       which the command list was created.
///     - The timestamp frequency can be queried from
///       ::ze_device_properties_t.timerResolution.
///     - The number of valid bits in the timestamp value can be queried from
///       ::ze_device_properties_t.timestampValidBits.
///     - The application must ensure the memory pointed to by dstptr is
///       accessible by the device on which the command list was created.
///     - The application must ensure the command list and events were created,
///       and the memory was allocated, on the same context.
///     - The application must **not** call this function from simultaneous
///       threads with the same command list handle.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hCommandList`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == dstptr`
///     - ::ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT
///     - ::ZE_RESULT_ERROR_INVALID_SIZE
///         + `(nullptr == phWaitEvents) && (0 < numWaitEvents)`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWriteGlobalTimestampExt(
    ze_command_list_handle_t hCommandList, ///< [in] handle of the command list
    uint64_t *dstptr,                      ///< [in,out] pointer to memory where timestamp value will be written; must
                                           ///< be 8byte-aligned.
    ze_event_handle_t hSignalEvent,        ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                ///< [in][optional] number of events to wait on before executing query;
                                           ///< must be 0 if `nullptr == phWaitEvents`
    ze_event_handle_t *phWaitEvents        ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                           ///< on before executing query
);

#ifndef ZE_MAX_EXTENSION_NAME
/// @brief Maximum extension name string size
#define ZE_MAX_EXTENSION_NAME 256
#endif // ZE_MAX_EXTENSION_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Extension properties queried using ::zeDriverGetExtensionProperties
typedef struct _ze_driver_extension_properties_t {
    char name[ZE_MAX_EXTENSION_NAME]; ///< [out] extension name
    uint32_t version;                 ///< [out] extension version using ::ZE_MAKE_VERSION

} ze_driver_extension_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieves extension properties
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @remarks
///   _Analogues_
///     - **vkEnumerateInstanceExtensionProperties**
///
///         + `nullptr == hDriver`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pCount`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetExtensionProperties(
    ze_driver_handle_t hDriver,                            ///< [in] handle of the driver instance
    uint32_t *pCount,                                      ///< [in,out] pointer to the number of extension properties.
                                                           ///< if count is zero, then the driver will update the value with the total
                                                           ///< number of extension properties available.
                                                           ///< if count is non-zero, then driver will only retrieve that number of
                                                           ///< extension properties.
                                                           ///< if count is larger than the number of extension properties available,
                                                           ///< then the driver will update the value with the correct number of
                                                           ///< extension properties available.
    ze_driver_extension_properties_t *pExtensionProperties ///< [in,out][optional][range(0, *pCount)] array of query results for
                                                           ///< extension properties
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Allocates shared memory on the context.
///
/// @details
///     - Shared allocations share ownership between the host and one or more
///       devices.
///     - Shared allocations may optionally be associated with a device by
///       passing a handle to the device.
///     - Devices supporting only single-device shared access capabilities may
///       access shared memory associated with the device.
///       For these devices, ownership of the allocation is shared between the
///       host and the associated device only.
///     - Passing nullptr as the device handle does not associate the shared
///       allocation with any device.
///       For allocations with no associated device, ownership of the allocation
///       is shared between the host and all devices supporting cross-device
///       shared access capabilities.
///     - The application must only use the memory allocation for the context
///       and device, or its sub-devices, which was provided during allocation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == deviceDesc`
///         + `nullptr == hostDesc`
///         + `nullptr == pptr`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x3 < deviceDesc->flags`
///         + `0x7 < hostDesc->flags`
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_SIZE
///         + `0 == size`
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT
///         + Must be zero or a power-of-two
///         + `0 != (alignment & (alignment - 1))`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocShared(
    ze_context_handle_t hContext,                 ///< [in] handle of the context object
    const ze_device_mem_alloc_desc_t *deviceDesc, ///< [in] pointer to device memory allocation descriptor
    const ze_host_mem_alloc_desc_t *hostDesc,     ///< [in] pointer to host memory allocation descriptor
    size_t size,                                  ///< [in] size in bytes to allocate; must be less-than
                                                  ///< ::ze_device_properties_t.maxMemAllocSize.
    size_t alignment,                             ///< [in] minimum alignment in bytes for the allocation; must be a power of
                                                  ///< two.
    ze_device_handle_t hDevice,                   ///< [in][optional] device handle to associate with
    void **pptr                                   ///< [out] pointer to shared allocation
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Allocates device memory on the context.
///
/// @details
///     - Device allocations are owned by a specific device.
///     - In general, a device allocation may only be accessed by the device
///       that owns it.
///     - The application must only use the memory allocation for the context
///       and device, or its sub-devices, which was provided during allocation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == deviceDesc`
///         + `nullptr == pptr`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x3 < deviceDesc->flags`
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_SIZE
///         + `0 == size`
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT
///         + Must be zero or a power-of-two
///         + `0 != (alignment & (alignment - 1))`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocDevice(
    ze_context_handle_t hContext,                 ///< [in] handle of the context object
    const ze_device_mem_alloc_desc_t *deviceDesc, ///< [in] pointer to device memory allocation descriptor
    size_t size,                                  ///< [in] size in bytes to allocate; must be less-than
                                                  ///< ::ze_device_properties_t.maxMemAllocSize.
    size_t alignment,                             ///< [in] minimum alignment in bytes for the allocation; must be a power of
                                                  ///< two.
    ze_device_handle_t hDevice,                   ///< [in] handle of the device
    void **pptr                                   ///< [out] pointer to device allocation
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Allocates host memory on the context.
///
/// @details
///     - Host allocations are owned by the host process.
///     - Host allocations are accessible by the host and all devices within the
///       driver's context.
///     - Host allocations are frequently used as staging areas to transfer data
///       to or from devices.
///     - The application must only use the memory allocation for the context
///       which was provided during allocation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == hostDesc`
///         + `nullptr == pptr`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x7 < hostDesc->flags`
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_SIZE
///         + `0 == size`
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT
///         + Must be zero or a power-of-two
///         + `0 != (alignment & (alignment - 1))`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocHost(
    ze_context_handle_t hContext,             ///< [in] handle of the context object
    const ze_host_mem_alloc_desc_t *hostDesc, ///< [in] pointer to host memory allocation descriptor
    size_t size,                              ///< [in] size in bytes to allocate; must be less-than
                                              ///< ::ze_device_properties_t.maxMemAllocSize.
    size_t alignment,                         ///< [in] minimum alignment in bytes for the allocation; must be a power of
                                              ///< two.
    void **pptr                               ///< [out] pointer to host allocation
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Frees allocated host memory, device memory, or shared memory on the
///        context.
///
/// @details
///     - The application must ensure the device is not currently referencing
///       the memory before it is freed
///     - The implementation of this function may immediately free all Host and
///       Device allocations associated with this memory
///     - The application must **not** call this function from simultaneous
///       threads with the same pointer.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == ptr`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemFree(
    ze_context_handle_t hContext, ///< [in] handle of the context object
    void *ptr                     ///< [in][release] pointer to memory to free
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieves attributes of a memory allocation
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The application may query attributes of a memory allocation unrelated
///       to the context.
///       When this occurs, the returned allocation type will be
///       ::ZE_MEMORY_TYPE_UNKNOWN, and the returned identifier and associated
///       device is unspecified.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == ptr`
///         + `nullptr == pMemAllocProperties`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAllocProperties(
    ze_context_handle_t hContext,                           ///< [in] handle of the context object
    const void *ptr,                                        ///< [in] memory pointer to query
    ze_memory_allocation_properties_t *pMemAllocProperties, ///< [in,out] query result for memory allocation properties
    ze_device_handle_t *phDevice                            ///< [out][optional] device associated with this allocation
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieves the base address and/or size of an allocation
///
/// @details
///     - The application may call this function from simultaneous threads.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == ptr`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAddressRange(
    ze_context_handle_t hContext, ///< [in] handle of the context object
    const void *ptr,              ///< [in] memory pointer to query
    void **pBase,                 ///< [in,out][optional] base address of the allocation
    size_t *pSize                 ///< [in,out][optional] size of the allocation
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Supported IPC memory flags
typedef uint32_t ze_ipc_memory_flags_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Opens an IPC memory handle to retrieve a device pointer on the
///        context.
///
/// @details
///     - Takes an IPC memory handle from a remote process and associates it
///       with a device pointer usable in this process.
///     - The device pointer in this process should not be freed with
///       ::zeMemFree, but rather with ::zeMemCloseIpcHandle.
///     - Multiple calls to this function with the same IPC handle will return
///       unique pointers.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x1 < flags`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pptr`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemOpenIpcHandle(
    ze_context_handle_t hContext, ///< [in] handle of the context object
    ze_device_handle_t hDevice,   ///< [in] handle of the device to associate with the IPC memory handle
    ze_ipc_mem_handle_t handle,   ///< [in] IPC memory handle
    ze_ipc_memory_flags_t flags,  ///< [in] flags controlling the operation.
                                  ///< must be 0 (default) or a valid combination of ::ze_ipc_memory_flag_t.
    void **pptr                   ///< [out] pointer to device allocation in this process
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Closes an IPC memory handle
///
/// @details
///     - Closes an IPC memory handle by unmapping memory that was opened in
///       this process using ::zeMemOpenIpcHandle.
///     - The application must **not** call this function from simultaneous
///       threads with the same pointer.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == ptr`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemCloseIpcHandle(
    ze_context_handle_t hContext, ///< [in] handle of the context object
    const void *ptr               ///< [in][release] pointer to device allocation in this process
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates an IPC memory handle for the specified allocation
///
/// @details
///     - Takes a pointer to a device memory allocation and creates an IPC
///       memory handle for exporting it for use in another process.
///     - The pointer must be base pointer of the device memory allocation; i.e.
///       the value returned from ::zeMemAllocDevice.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == ptr`
///         + `nullptr == pIpcHandle`
ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetIpcHandle(
    ze_context_handle_t hContext,   ///< [in] handle of the context object
    const void *ptr,                ///< [in] pointer to the device memory allocation
    ze_ipc_mem_handle_t *pIpcHandle ///< [out] Returned IPC memory handle
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates a module on the context.
///
/// @details
///     - Compiles the module for execution on the device.
///     - The application must only use the module for the device, or its
///       sub-devices, which was provided during creation.
///     - The module can be copied to other devices and contexts within the same
///       driver instance by using ::zeModuleGetNativeBinary.
///     - A build log can optionally be returned to the caller. The caller is
///       responsible for destroying build log using ::zeModuleBuildLogDestroy.
///     - The module descriptor constants are only supported for SPIR-V
///       specialization constants.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == desc`
///         + `nullptr == desc->pInputModule`
///         + `nullptr == phModule`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `::ZE_MODULE_FORMAT_NATIVE < desc->format`
///     - ::ZE_RESULT_ERROR_INVALID_NATIVE_BINARY
///     - ::ZE_RESULT_ERROR_INVALID_SIZE
///         + `0 == desc->inputSize`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_MODULE_BUILD_FAILURE
ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleCreateExt(
    ze_context_handle_t hContext,            ///< [in] handle of the context object
    ze_device_handle_t hDevice,              ///< [in] handle of the device
    const ze_module_desc_t *desc,            ///< [in] pointer to module descriptor
    ze_module_handle_t *phModule,            ///< [out] pointer to handle of module object created
    ze_module_build_log_handle_t *phBuildLog ///< [out][optional] pointer to handle of module's build log.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates sampler on the context.
///
/// @details
///     - The application must only use the sampler for the device, or its
///       sub-devices, which was provided during creation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == desc`
///         + `nullptr == phSampler`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `::ZE_SAMPLER_ADDRESS_MODE_MIRROR < desc->addressMode`
///         + `::ZE_SAMPLER_FILTER_MODE_LINEAR < desc->filterMode`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerCreateExt(
    ze_context_handle_t hContext,  ///< [in] handle of the context object
    ze_device_handle_t hDevice,    ///< [in] handle of the device
    const ze_sampler_desc_t *desc, ///< [in] pointer to sampler descriptor
    ze_sampler_handle_t *phSampler ///< [out] handle of the sampler
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates a command queue on the context.
///
/// @details
///     - A command queue represents a logical input stream to the device, tied
///       to a physical input stream.
///     - The application must only use the command queue for the device, or its
///       sub-devices, which was provided during creation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @remarks
///   _Analogues_
///     - **clCreateCommandQueue**
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == desc`
///         + `nullptr == phCommandQueue`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x1 < desc->flags`
///         + `::ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS < desc->mode`
///         + `::ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH < desc->priority`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueCreateExt(
    ze_context_handle_t hContext,             ///< [in] handle of the context object
    ze_device_handle_t hDevice,               ///< [in] handle of the device object
    const ze_command_queue_desc_t *desc,      ///< [in] pointer to command queue descriptor
    ze_command_queue_handle_t *phCommandQueue ///< [out] pointer to handle of command queue object created
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates a command list on the context.
///
/// @details
///     - A command list represents a sequence of commands for execution on a
///       command queue.
///     - The command list is created in the 'open' state.
///     - The application must only use the command list for the device, or its
///       sub-devices, which was provided during creation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == desc`
///         + `nullptr == phCommandList`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x7 < desc->flags`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateExt(
    ze_context_handle_t hContext,           ///< [in] handle of the context object
    ze_device_handle_t hDevice,             ///< [in] handle of the device object
    const ze_command_list_desc_t *desc,     ///< [in] pointer to command list descriptor
    ze_command_list_handle_t *phCommandList ///< [out] pointer to handle of command list object created
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates an immediate command list on the context.
///
/// @details
///     - An immediate command list is used for low-latency submission of
///       commands.
///     - An immediate command list creates an implicit command queue.
///     - The command list is created in the 'open' state and never needs to be
///       closed.
///     - The application must only use the command list for the device, or its
///       sub-devices, which was provided during creation.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == altdesc`
///         + `nullptr == phCommandList`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `0x1 < altdesc->flags`
///         + `::ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS < altdesc->mode`
///         + `::ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH < altdesc->priority`
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateImmediateExt(
    ze_context_handle_t hContext,           ///< [in] handle of the context object
    ze_device_handle_t hDevice,             ///< [in] handle of the device object
    const ze_command_queue_desc_t *altdesc, ///< [in] pointer to command queue descriptor
    ze_command_list_handle_t *phCommandList ///< [out] pointer to handle of command list object created
);

} //extern C

#endif // _ZE_API_EXT_H
