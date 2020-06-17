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

} //extern C

#endif // _ZE_API_EXT_H
