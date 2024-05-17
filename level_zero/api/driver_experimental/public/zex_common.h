/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_COMMON_H
#define _ZEX_COMMON_H
#if defined(__cplusplus)
#pragma once
#endif
#include <level_zero/ze_api.h>

#if defined(__cplusplus)
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of command list object
typedef ze_command_list_handle_t zex_command_list_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of event object
typedef ze_event_handle_t zex_event_handle_t;

#define ZEX_BIT(_i) (1 << _i)

typedef uint32_t zex_mem_action_scope_flags_t;
typedef enum _zex_mem_action_scope_flag_t {
    ZEX_MEM_ACTION_SCOPE_FLAG_SUBDEVICE = ZEX_BIT(0),
    ZEX_MEM_ACTION_SCOPE_FLAG_DEVICE = ZEX_BIT(1),
    ZEX_MEM_ACTION_SCOPE_FLAG_HOST = ZEX_BIT(2),
    ZEX_MEM_ACTION_SCOPE_FLAG_FORCE_UINT32 = 0x7fffffff
} zex_mem_action_scope_flag_t;

typedef uint32_t zex_wait_on_mem_action_flags_t;
typedef enum _zex_wait_on_mem_action_flag_t {
    ZEX_WAIT_ON_MEMORY_FLAG_EQUAL = ZEX_BIT(0),
    ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL = ZEX_BIT(1),
    ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN = ZEX_BIT(2),
    ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN_EQUAL = ZEX_BIT(3),
    ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN = ZEX_BIT(4),
    ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL = ZEX_BIT(5),
    ZEX_WAIT_ON_MEMORY_FLAG_FORCE_UINT32 = 0x7fffffff
} zex_wait_on_mem_action_flag_t;

typedef struct _zex_wait_on_mem_desc_t {
    zex_wait_on_mem_action_flags_t actionFlag;
    zex_mem_action_scope_flags_t waitScope;
} zex_wait_on_mem_desc_t;

typedef struct _zex_write_to_mem_desc_t {
    zex_mem_action_scope_flags_t writeScope;
} zex_write_to_mem_desc_t;

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_SYNCHRONIZED_DISPATCH_EXP_NAME
/// @brief Synchronized Dispatch extension name
#define ZE_SYNCHRONIZED_DISPATCH_EXP_NAME "ZE_experimental_synchronized_dispatch"
#endif // ZE_SYNCHRONIZED_DISPATCH_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Synchronized Dispatch extension version(s)
typedef enum _ze_synchronized_dispatch_exp_version_t {
    ZE_SYNCHRONIZED_DISPATCH_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZE_SYNCHRONIZED_DISPATCH_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZE_SYNCHRONIZED_DISPATCH_EXP_VERSION_FORCE_UINT32 = 0x7fffffff

} ze_synchronized_dispatch_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Supported synchronized dispatch flags
typedef uint32_t ze_synchronized_dispatch_exp_flags_t;
typedef enum _ze_synchronized_dispatch_exp_flag_t {
    ZE_SYNCHRONIZED_DISPATCH_DISABLED_EXP_FLAG = ZE_BIT(0), ///< Non-synchronized dispatch. Must synchronize only with other synchronized dispatches
    ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG = ZE_BIT(1),  ///< Synchronized dispatch. Must synchronize with all synchronized and non-synchronized dispatches
    ZE_SYNCHRONIZED_DISPATCH_EXP_FLAG_FORCE_UINT32 = 0x7fffffff

} ze_synchronized_dispatch_exp_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare ze_synchronized_dispatch_exp_desc_t
typedef struct _ze_synchronized_dispatch_exp_desc_t ze_synchronized_dispatch_exp_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Command queue or command list descriptor for synchronized dispatch. This structure may be
///        passed as pNext member of ::ze_command_queue_desc_t. or ::ze_command_list_desc_t.
typedef struct _ze_synchronized_dispatch_exp_desc_t {
    ze_structure_type_t stype;                  ///< [in] type of this structure
    const void *pNext;                          ///< [in][optional] must be null or a pointer to an extension-specific
                                                ///< structure (i.e. contains stype and pNext).
    ze_synchronized_dispatch_exp_flags_t flags; ///< [in] mode flags.
                                                ///< must be valid value of ::ze_synchronized_dispatch_exp_flag_t

} ze_synchronized_dispatch_exp_desc_t;

#define ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC (ze_structure_type_t)0x00020020

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare ze_intel_media_communication_desc_t
typedef struct _ze_intel_media_communication_desc_t ze_intel_media_communication_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief ze_intel_media_communication_desc_t
typedef struct _ze_intel_media_communication_desc_t {
    ze_structure_type_t stype;              ///< [in] type of this structure
    void *pNext;                            ///< [in][optional] must be null or a pointer to an extension-specific, this will be used to extend this in future
    void *controlSharedMemoryBuffer;        ///< [in] control shared memory buffer pointer, must be USM address
    uint32_t controlSharedMemoryBufferSize; ///< [in] control shared memory buffer size
    void *controlBatchBuffer;               ///< [in] control batch buffer pointer, must be USM address
    uint32_t controlBatchBufferSize;        ///< [in] control batch buffer size
} ze_intel_media_communication_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare ze_intel_media_doorbell_handle_desc_t
typedef struct _ze_intel_media_doorbell_handle_desc_t ze_intel_media_doorbell_handle_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief ze_intel_media_doorbell_handle_desc_t
/// @details Handle of the doorbell. This structure is passed as argument of zeIntelMediaCommunicationCreate and zeIntelMediaCommunicationDestroy
typedef struct _ze_intel_media_doorbell_handle_desc_t {
    ze_structure_type_t stype; ///< [in] type of this structure
    void *pNext;               ///< [in][optional] must be null or a pointer to an extension-specific, this will be used to extend this in future
    uint64_t doorbell;         ///< [in,out] handle of the doorbell
} ze_intel_media_doorbell_handle_desc_t;

#define ZE_STRUCTURE_TYPE_INTEL_MEDIA_COMMUNICATION_DESC (ze_structure_type_t)0x00020021
#define ZE_STRUCTURE_TYPE_INTEL_MEDIA_DOORBELL_HANDLE_DESC (ze_structure_type_t)0x00020022

///////////////////////////////////////////////////////////////////////////////
/// @brief Supported device media flags
typedef uint32_t ze_intel_device_media_exp_flags_t;
typedef enum _ze_intel_device_media_exp_flag_t {
    ZE_INTEL_DEVICE_MEDIA_SUPPORTS_ENCODING_EXP_FLAG = ZE_BIT(0), ///< Supports encoding
    ZE_INTEL_DEVICE_MEDIA_SUPPORTS_DECODING_EXP_FLAG = ZE_BIT(1), ///< Supports decoding
    ZE_INTEL_DEVICE_MEDIA_EXP_FLAG_FORCE_UINT32 = 0x7fffffff
} ze_intel_device_media_exp_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare ze_intel_device_media_exp_properties_t
typedef struct _ze_intel_device_media_exp_properties_t ze_intel_device_media_exp_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief  May be passed to ze_device_properties_t through pNext.
typedef struct _ze_intel_device_media_exp_properties_t {
    ze_structure_type_t stype;               ///< [in] type of this structure
    const void *pNext;                       ///< [in][optional] must be null or a pointer to an extension-specific
    ze_intel_device_media_exp_flags_t flags; ///< [out] device media flags
    uint32_t numEncoderCores;                ///< [out] number of encoder cores
    uint32_t numDecoderCores;                ///< [out] number of decoder cores
} ze_intel_device_media_exp_properties_t;

#define ZE_STRUCTURE_TYPE_INTEL_DEVICE_MEDIA_EXP_PROPERTIES (ze_structure_type_t)0x00020023

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZEX_COMMON_EXTENDED_H
