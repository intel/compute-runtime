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

} //extern C

#endif // _ZE_API_EXT_H
