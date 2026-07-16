/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/core/ze_mutable_cmdlist_api_entrypoints.h"
#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeCommandListAppendHostFunction(
    ze_command_list_handle_t hCommandList,
    ze_host_function_callback_t pHostFunction,
    void *pUserData,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList);

ze_result_t ZE_APICALL zeCommandListCreateImmediate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList);

ze_result_t ZE_APICALL zeCommandListDestroy(
    ze_command_list_handle_t hCommandList);

ze_result_t ZE_APICALL zeCommandListClose(
    ze_command_list_handle_t hCommandList);

ze_result_t ZE_APICALL zeCommandListReset(
    ze_command_list_handle_t hCommandList);

ze_result_t ZE_APICALL zeCommandListAppendWriteGlobalTimestamp(
    ze_command_list_handle_t hCommandList,
    uint64_t *dstptr,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendQueryKernelTimestamps(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents,
    void *dstptr,
    const size_t *pOffsets,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListGetDeviceHandle(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t *phDevice);

ze_result_t ZE_APICALL zeCommandListGetContextHandle(
    ze_command_list_handle_t hCommandList,
    ze_context_handle_t *phContext);

ze_result_t ZE_APICALL zeCommandListGetOrdinal(
    ze_command_list_handle_t hCommandList,
    uint32_t *pOrdinal);

ze_result_t ZE_APICALL zeCommandListGetFlags(
    ze_command_list_handle_t hCommandList,
    ze_command_list_flags_t *pFlags);

ze_result_t ZE_APICALL zeCommandListImmediateGetIndex(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t *pIndex);

ze_result_t ZE_APICALL zeCommandListImmediateGetFlags(
    ze_command_list_handle_t hCommandListImmediate,
    ze_command_queue_flags_t *pFlags);

ze_result_t ZE_APICALL zeCommandListImmediateGetMode(
    ze_command_list_handle_t hCommandListImmediate,
    ze_command_queue_mode_t *pMode);

ze_result_t ZE_APICALL zeCommandListImmediateGetPriority(
    ze_command_list_handle_t hCommandListImmediate,
    ze_command_queue_priority_t *pPriority);

ze_result_t ZE_APICALL zeCommandListIsImmediate(
    ze_command_list_handle_t hCommandList,
    ze_bool_t *pIsImmediate);

ze_result_t ZE_APICALL zeCommandListCreateCloneExp(
    ze_command_list_handle_t hCommandList,
    ze_command_list_handle_t *phClonedCommandList);

ze_result_t ZE_APICALL zeCommandListImmediateAppendCommandListsExp(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendSignalExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_signal_params_ext_t *signalParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendWaitExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_wait_params_ext_t *waitParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelWithArguments(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t groupCounts,
    const ze_group_size_t groupSizes,
    void **pArguments,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelWithParameters(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pGroupCounts,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListImmediateAppendCommandListsWithParameters(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendMemoryCopyWithParameters(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendMemoryFillWithParameters(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListCreateImmediate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListDestroy(
    ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListClose(
    ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListReset(
    ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendWriteGlobalTimestamp(
    ze_command_list_handle_t hCommandList,
    uint64_t *dstptr,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendQueryKernelTimestamps(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents,
    void *dstptr,
    const size_t *pOffsets,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetDeviceHandle(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t *phDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetContextHandle(
    ze_command_list_handle_t hCommandList,
    ze_context_handle_t *phContext);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetOrdinal(
    ze_command_list_handle_t hCommandList,
    uint32_t *pOrdinal);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateGetIndex(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t *pIndex);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListIsImmediate(
    ze_command_list_handle_t hCommandList,
    ze_bool_t *pIsImmediate);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateAppendCommandListsWithParameters(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateAppendCommandListsExp(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendSignalExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_signal_params_ext_t *signalParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendWaitExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_wait_params_ext_t *waitParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelWithArguments(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t groupCounts,
    const ze_group_size_t groupSizes,
    void **pArguments,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelWithParameters(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pGroupCounts,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryCopyWithParameters(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryFillWithParameters(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateGetFlags(
    ze_command_list_handle_t hCommandList,
    ze_command_queue_flags_t *pFlags);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateGetMode(
    ze_command_list_handle_t hCommandList,
    ze_command_queue_mode_t *pMode);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateGetPriority(
    ze_command_list_handle_t hCommandList,
    ze_command_queue_priority_t *pPriority);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendHostFunction(
    ze_command_list_handle_t hCommandList,
    ze_host_function_callback_t pfnHostFunction,
    void *pUserData,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

} // extern "C"
