/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent);

ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory64(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint64_t data,
    zex_event_handle_t hSignalEvent);

ze_result_t ZE_APICALL
zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data);

ze_result_t ZE_APICALL
zeCommandListAppendHostFunction(
    ze_command_list_handle_t hCommandList,
    ze_host_function_callback_t pHostFunction,
    void *pUserData,
    void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL
zexCommandListAppendMemoryCopyWithParameters(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    const void *pNext,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents,
    ze_event_handle_t hSignalEvent);

ze_result_t ZE_APICALL
zexCommandListAppendMemoryFillWithParameters(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    const void *pNext,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL
zexCommandListAppendCustomOperation(
    ze_command_list_handle_t hCommandList,
    const void *pNext,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL
zexCommandListSetCleanupCallback(
    ze_command_list_handle_t hCommandList,
    zex_command_list_cleanup_callback_fn_t pfnCallback,
    void *pUserData,
    const void *pNext);

ze_result_t ZE_APICALL
zexCommandListVerifyMemory(
    ze_command_list_handle_t hCommandList,
    const void *allocationPtr,
    const void *expectedData,
    size_t sizeOfComparison,
    zex_verify_memory_compare_type_t comparisonMode);

} // namespace L0
