/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_GRAPH_H
#define _ZEX_GRAPH_H
#if defined(__cplusplus)
#pragma once
#endif

#include <level_zero/ze_api.h>

#include "zex_common.h"

#ifndef ZE_RECORD_REPLAY_GRAPH_EXP_NAME
/// @brief Record and Replay Graph Extension Name
#define ZE_RECORD_REPLAY_GRAPH_EXP_NAME "ZE_experimental_record_replay_graph"

typedef enum _ze_record_replay_graph_exp_version_t {
    ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_1_1 = ZE_MAKE_VERSION(1, 1),     ///< version 1.1
    ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 1), ///< latest known version
    ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_FORCE_UINT32 = 0x7fffffff,       ///< Value marking end of ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_* ENUMs
} ze_record_replay_graph_exp_version_t;

typedef uint32_t ze_record_replay_graph_exp_flags_t;
typedef enum _ze_record_replay_graph_exp_flag_t {
    ZE_RECORD_REPLAY_GRAPH_EXP_FLAG_IMMUTABLE_GRAPH = ZE_BIT(0),         ///< immutable
    ZE_RECORD_REPLAY_GRAPH_EXP_FLAG_MUTABLE_GRAPH = ZE_BIT(1),           ///< mutable
    ZE_RECORD_REPLAY_GRAPH_EXP_FLAG_UPDATE_FROM_OTHER_GRAPH = ZE_BIT(5), ///< supports updating an executable graph from any recorded graph of the same topology
    ZE_RECORD_REPLAY_GRAPH_EXP_FLAG_FORCE_UINT32 = 0x7fffffff,           ///< Value marking end of ZE_RECORD_REPLAY_EXP_FLAG_* ENUMs
} ze_record_replay_graph_exp_flag_t;

typedef struct _ze_record_replay_graph_exp_properties_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure
    void *pNext;                   ///< [in,out][optional] must be null or a pointer to an extension-specific
    ///< structure (i.e. contains stype and pNext).
    ze_record_replay_graph_exp_flags_t graphFlags; ///< [out] record replay flags
} ze_record_replay_graph_exp_properties_t;

typedef struct _ze_graph_handle_t *ze_graph_handle_t;
typedef struct _ze_executable_graph_handle_t *ze_executable_graph_handle_t;

typedef enum _ze_record_replay_graph_exp_dump_mode_t {
    ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_DETAILED = 0x0,                  ///< detailed mode, internal dependency-tracking event operations are not dumped (default)
    ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_SIMPLE = 0x1,                    ///< simple mode, internal dependency-tracking event operations are not dumped
    ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_DETAILED_WITH_EVENT_NODES = 0x2, ///< detailed mode with all event operations dumped as graph nodes
    ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_SIMPLE_WITH_EVENT_NODES = 0x3,   ///< simple mode with all event operations dumped as graph nodes
    ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_FORCE_UINT32 = 0x7fffffff,       ///< Value marking end of ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_* ENUMs
} ze_record_replay_graph_exp_dump_mode_t;

typedef struct _ze_record_replay_graph_exp_dump_desc_t {
    ze_structure_type_ext_t stype;               ///< [in] type of this structure
    const void *pNext;                           ///< [in][optional] must be null or a pointer to an extension-specific
                                                 ///< structure (i.e. contains stype and pNext).
    ze_record_replay_graph_exp_dump_mode_t mode; ///< [in] graph dump mode
} ze_record_replay_graph_exp_dump_desc_t;

#define ZE_RESULT_QUERY_TRUE_EXP EXTENDED_ENUM(ze_result_t, 0x7fff0000)
#define ZE_RESULT_QUERY_FALSE_EXP EXTENDED_ENUM(ze_result_t, 0x7fff0001)
#define ZE_RESULT_ERROR_INVALID_GRAPH_EXP EXTENDED_ENUM(ze_result_t, 0x7fff0002)
#define ZE_RESULT_ERROR_INVALID_OPERATION_EXP EXTENDED_ENUM(ze_result_t, 0x7fff0004)

typedef uint32_t ze_event_exp_flags_t;
typedef enum _ze_event_exp_flag_t {
    ZE_EVENT_EXP_FLAG_APPEND_GRAPH_EXTERNAL = ZE_BIT(0), ///< event created without ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL flag will behave as a ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL event for this particular append operation. Using this flag on event created with flag ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL has no effect.
    ZE_EVENT_EXP_FLAG_FORCE_UINT32 = 0x7fffffff,
} ze_event_exp_flag_t;

typedef struct _ze_event_exp_flags_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_EVENT_EXP_FLAGS_DESC
    const void *pNext;             ///< [in,out][optional] must be null or a pointer to an extension-specific
    ze_event_exp_flags_t flags;
} ze_event_exp_flags_desc_t;

typedef struct _ze_graph_node_handle_t *ze_graph_node_handle_t;

typedef enum _ze_graph_node_exp_type_t {
    ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL = 0,         ///< node represents launch kernel operation
    ZE_GRAPH_NODE_EXP_TYPE_FORCE_UINT32 = 0x7fffffff, ///< Value marking end of ZE_GRAPH_NODE_EXP_TYPE_* ENUMs
} ze_graph_node_exp_type_t;

typedef uint32_t ze_mutable_graph_node_exp_flags_t;
typedef enum _ze_mutable_graph_node_exp_flag_t {
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT = ZE_BIT(0),    ///< node is allowed to mutate kernel arguments
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT = ZE_BIT(1),        ///< node is allowed to mutate group count
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE = ZE_BIT(2),         ///< node is allowed to mutate group size
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET = ZE_BIT(3),      ///< node is allowed to mutate global offset
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_SIGNAL_EVENT = ZE_BIT(4),       ///< node is allowed to mutate signal event
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS = ZE_BIT(5),        ///< node is allowed to mutate wait events
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION = ZE_BIT(6), ///< node is allowed to mutate kernel instruction, KERNEL_ARGUMENT (when kernel
                                                                   ///< has arguments), GROUP_COUNT and GROUP_SIZE must also be selected
    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_FORCE_UINT32 = 0x7fffffff,      ///< Value marking end of ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_* ENUMs
} ze_mutable_graph_node_exp_flag_t;

typedef struct _ze_mutable_graph_exp_properties_t {
    ze_structure_type_ext_t stype;           ///< [in] type of this structure ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_PROPERTIES
    void *pNext;                             ///< [in,out][optional] must be null or a pointer to an extension-specific
                                             ///< structure (i.e. contains stype and pNext).
    ze_mutable_graph_node_exp_flags_t flags; ///< [out] mutable operations supported by the device
} ze_mutable_graph_exp_properties_t;

typedef struct _ze_graph_node_exp_properties_t {
    ze_structure_type_ext_t stype;           ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_PROPERTIES
    void *pNext;                             ///< [in,out][optional] must be null or a pointer to an extension-specific
                                             ///< structure (i.e. contains stype and pNext).
    ze_graph_node_exp_type_t type;           ///< [out] type of graph node
    ze_mutable_graph_node_exp_flags_t flags; ///< [out] combination of flags used at graph node creation
} ze_graph_node_exp_properties_t;

typedef struct _ze_mutable_graph_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
} ze_mutable_graph_exp_desc_t;

typedef struct _ze_graph_node_exp_desc_t {
    ze_structure_type_ext_t stype;           ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC
    const void *pNext;                       ///< [in][optional] must be null or a pointer to an extension-specific
                                             ///< structure (i.e. contains stype and pNext).
    ze_mutable_graph_node_exp_flags_t flags; ///< [in] mutable flags that select which mutations are allowed on this node,
                                             ///< 0 selects all flags supported by the device except kernel instruction
    ze_graph_node_handle_t *phNodeHandle;    ///< [out] pointer to node handle that will be created and initialized by the driver
} ze_graph_node_exp_desc_t;

typedef struct _ze_mutable_kernel_data_exp_desc_t {
    ze_structure_type_ext_t stype;     ///< [in] type of this structure ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_DATA_EXP_DESC
    const void *pNext;                 ///< [in][optional] must be null or a pointer to an extension-specific
                                       ///< structure (i.e. contains stype and pNext).
    uint32_t numKernels;               ///< [in] number of kernels in kernel group passed in phKernelGroup
    ze_kernel_handle_t *phKernelGroup; ///< [in][range(0, numKernels)] pointer to array of kernel handles defining a kernel group
} ze_mutable_kernel_data_exp_desc_t;

typedef struct _ze_graph_node_update_launch_kernel_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    ze_kernel_handle_t hKernel;    ///< [in] new kernel, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION
    const size_t *pArgSizes;       ///< [in][range(0, number of kernel arguments)] kernel argument sizes, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT
    const void **pArgValues;       ///< [in][range(0, number of kernel arguments)] kernel argument values, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT
    ze_group_count_t groupCount;   ///< [in] new group count, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT
    ze_group_size_t groupSize;     ///< [in] new group size, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE
} ze_graph_node_update_launch_kernel_exp_desc_t;

typedef struct _ze_graph_node_update_kernel_argument_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    uint32_t argIndex;             ///< [in] index of kernel argument
    size_t argSize;                ///< [in] size of kernel argument
    const void *pArgValue;         ///< [in] pointer to kernel argument
} ze_graph_node_update_kernel_argument_exp_desc_t;

typedef struct _ze_graph_node_update_group_count_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    ze_group_count_t groupCount;   ///< [in] new group count of launched kernel
} ze_graph_node_update_group_count_exp_desc_t;

typedef struct _ze_graph_node_update_group_size_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    ze_group_size_t groupSize;     ///< [in] new group size of launched kernel
} ze_graph_node_update_group_size_exp_desc_t;

typedef struct _ze_graph_node_update_global_offset_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    uint32_t offsetX;              ///< [in] offset of launched kernel in X dimension
    uint32_t offsetY;              ///< [in] offset of launched kernel in Y dimension
    uint32_t offsetZ;              ///< [in] offset of launched kernel in Z dimension
} ze_graph_node_update_global_offset_exp_desc_t;

typedef struct _ze_graph_node_update_signal_event_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_SIGNAL_EVENT_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    ze_event_handle_t hEvent;      ///< [in] new signal event handle
} ze_graph_node_update_signal_event_exp_desc_t;

typedef struct _ze_graph_node_update_wait_events_exp_desc_t {
    ze_structure_type_ext_t stype; ///< [in] type of this structure ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_WAIT_EVENTS_EXP_DESC
    const void *pNext;             ///< [in][optional] must be null or a pointer to an extension-specific
                                   ///< structure (i.e. contains stype and pNext).
    uint32_t numEvents;            ///< [in] number of wait events, must be the same as used during graph recording
    ze_event_handle_t *phEvents;   ///< [in][range(0, numEvents)] array of wait event handles
} ze_graph_node_update_wait_events_exp_desc_t;

#if defined(__cplusplus)
extern "C" {
#endif

typedef void(ZE_CALLBACK *zex_mem_graph_free_callback_fn_t)(void *pUserData);

ze_result_t ZE_APICALL zeGraphCreateExp(ze_context_handle_t hContext, ze_graph_handle_t *phGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExp(ze_command_list_handle_t hCommandList, void *pNext);
ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListInstantiateGraphExp(ze_graph_handle_t hGraph, ze_executable_graph_handle_t *phExecutableGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListAppendGraphExp(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, void *pNext,
                                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t ZE_APICALL zeGraphDestroyExp(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeExecutableGraphDestroyExp(ze_executable_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExp(ze_command_list_handle_t hCommandList);
ze_result_t ZE_APICALL zeGraphIsEmptyExp(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeGraphDumpContentsExp(ze_graph_handle_t hGraph, const char *filePath, void *pNext);
ze_result_t ZE_APICALL zeCommandListGetGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph);
ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExp(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, void *pNext);
ze_result_t ZE_APICALL zeGraphGetNodesExp(ze_graph_handle_t hGraph, uint32_t *pGraphNodesCount, ze_graph_node_handle_t *phGraphNodes);
ze_result_t ZE_APICALL zeGraphNodeGetPropertiesExp(ze_graph_node_handle_t hNode, ze_graph_node_exp_properties_t *pProperties);
ze_result_t ZE_APICALL zeGraphNodesUpdateExp(ze_graph_handle_t hGraph, uint32_t numNodes, ze_graph_node_handle_t *phNodes, const void **ppUpdateDescs, const void *pNext);
ze_result_t ZE_APICALL zeExecutableGraphNodesUpdateExp(ze_executable_graph_handle_t hExecutableGraph, uint32_t numNodes, ze_graph_node_handle_t *phNodes, const void **ppUpdateDescs, const void *pNext);
ze_result_t ZE_APICALL zeExecutableGraphUpdateExp(ze_executable_graph_handle_t hExecutableGraph, ze_graph_handle_t hSourceGraph, const void *pNext);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // ZE_RECORD_REPLAY_GRAPH_EXP_NAME
#endif // _ZEX_GRAPH_H
