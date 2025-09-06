/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/core/ze_mutable_cmdlist_api_entrypoints.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeCommandListCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList) {
    return L0::Context::fromHandle(hContext)->createCommandList(hDevice, desc, phCommandList);
}

ze_result_t zeCommandListCreateImmediate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList) {
    return L0::Context::fromHandle(hContext)->createCommandListImmediate(hDevice, altdesc, phCommandList);
}

ze_result_t zeCommandListDestroy(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->destroy();
}

ze_result_t zeCommandListClose(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->close();
}

ze_result_t zeCommandListReset(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->reset();
}

ze_result_t zeCommandListAppendWriteGlobalTimestamp(
    ze_command_list_handle_t hCommandList,
    uint64_t *dstptr,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(hCommandList, dstptr, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeCommandListAppendQueryKernelTimestamps(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents,
    void *dstptr,
    const size_t *pOffsets,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(hCommandList, numEvents, phEvents, dstptr, pOffsets, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendQueryKernelTimestamps(numEvents, phEvents, dstptr, pOffsets, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeCommandListGetDeviceHandle(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t *phDevice) {
    return L0::CommandList::fromHandle(hCommandList)->getDeviceHandle(phDevice);
}

ze_result_t zeCommandListGetContextHandle(
    ze_command_list_handle_t hCommandList,
    ze_context_handle_t *phContext) {
    return L0::CommandList::fromHandle(hCommandList)->getContextHandle(phContext);
}

ze_result_t zeCommandListGetOrdinal(
    ze_command_list_handle_t hCommandList,
    uint32_t *pOrdinal) {
    return L0::CommandList::fromHandle(hCommandList)->getOrdinal(pOrdinal);
}

ze_result_t zeCommandListImmediateGetIndex(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t *pIndex) {
    return L0::CommandList::fromHandle(hCommandListImmediate)->getImmediateIndex(pIndex);
}

ze_result_t zeCommandListIsImmediate(
    ze_command_list_handle_t hCommandList,
    ze_bool_t *pIsImmediate) {
    return L0::CommandList::fromHandle(hCommandList)->isImmediate(pIsImmediate);
}

ze_result_t zeCommandListCreateCloneExp(
    ze_command_list_handle_t hCommandList,
    ze_command_list_handle_t *phClonedCommandList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ZE_APICALL zeCommandListImmediateAppendCommandListsExp(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandListImmediate)->appendCommandLists(numCommandLists, phCommandLists, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeCommandListAppendSignalExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_signal_params_ext_t *signalParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(hCommandList, numSemaphores, phSemaphores, signalParams, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendSignalExternalSemaphores(numSemaphores, phSemaphores, signalParams, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeCommandListAppendWaitExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_wait_params_ext_t *waitParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>(hCommandList, numSemaphores, phSemaphores, waitParams, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendWaitExternalSemaphores(numSemaphores, phSemaphores, waitParams, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeCommandListAppendLaunchKernelWithArguments(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t groupCounts,
    const ze_group_size_t groupSizes,
    void **pArguments,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    if (!hCommandList) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(hCommandList, hKernel, groupCounts, groupSizes, pArguments, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchKernelWithArguments(hKernel, groupCounts, groupSizes, pArguments, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeCommandListAppendLaunchKernelWithParameters(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pGroupCounts,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    if (!hCommandList) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(hCommandList, hKernel, pGroupCounts, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendLaunchKernelWithParameters(hKernel, pGroupCounts, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}
} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList) {
    return L0::zeCommandListCreate(
        hContext,
        hDevice,
        desc,
        phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListCreateImmediate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList) {
    return L0::zeCommandListCreateImmediate(
        hContext,
        hDevice,
        altdesc,
        phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListDestroy(
    ze_command_list_handle_t hCommandList) {
    return L0::zeCommandListDestroy(
        hCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListClose(
    ze_command_list_handle_t hCommandList) {
    return L0::zeCommandListClose(
        hCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListReset(
    ze_command_list_handle_t hCommandList) {
    return L0::zeCommandListReset(
        hCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendWriteGlobalTimestamp(
    ze_command_list_handle_t hCommandList,
    uint64_t *dstptr,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendWriteGlobalTimestamp(
        hCommandList,
        dstptr,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetDeviceHandle(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t *phDevice) {
    return L0::zeCommandListGetDeviceHandle(
        hCommandList,
        phDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetContextHandle(
    ze_command_list_handle_t hCommandList,
    ze_context_handle_t *phContext) {
    return L0::zeCommandListGetContextHandle(
        hCommandList,
        phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetOrdinal(
    ze_command_list_handle_t hCommandList,
    uint32_t *pOrdinal) {
    return L0::zeCommandListGetOrdinal(
        hCommandList,
        pOrdinal);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateGetIndex(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t *pIndex) {
    return L0::zeCommandListImmediateGetIndex(
        hCommandListImmediate,
        pIndex);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListIsImmediate(
    ze_command_list_handle_t hCommandList,
    ze_bool_t *pIsImmediate) {
    return L0::zeCommandListIsImmediate(
        hCommandList,
        pIsImmediate);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListImmediateAppendCommandListsExp(
    ze_command_list_handle_t hCommandListImmediate,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListImmediateAppendCommandListsExp(
        hCommandListImmediate,
        numCommandLists,
        phCommandLists,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendSignalExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_signal_params_ext_t *signalParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendSignalExternalSemaphoreExt(hCommandList, numSemaphores, phSemaphores, signalParams, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendWaitExternalSemaphoreExt(
    ze_command_list_handle_t hCommandList,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_wait_params_ext_t *waitParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendWaitExternalSemaphoreExt(hCommandList, numSemaphores, phSemaphores, waitParams, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelWithArguments(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t groupCounts,
    const ze_group_size_t groupSizes,
    void **pArguments,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendLaunchKernelWithArguments(hCommandList, hKernel, groupCounts, groupSizes, pArguments, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelWithParameters(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pGroupCounts,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    return L0::zeCommandListAppendLaunchKernelWithParameters(
        hCommandList, hKernel, pGroupCounts, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}
} // extern "C"
