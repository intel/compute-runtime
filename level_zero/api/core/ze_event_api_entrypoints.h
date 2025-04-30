/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/event/event.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeEventPoolCreate(
    ze_context_handle_t hContext,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool) {
    return L0::Context::fromHandle(hContext)->createEventPool(desc, numDevices, phDevices, phEventPool);
}

ze_result_t zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool) {
    return L0::EventPool::fromHandle(hEventPool)->destroy();
}

ze_result_t zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent) {
    return L0::EventPool::fromHandle(hEventPool)->createEvent(desc, phEvent);
}

ze_result_t zeEventDestroy(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->destroy();
}

ze_result_t zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc) {
    return L0::EventPool::fromHandle(hEventPool)->getIpcHandle(phIpc);
}

ze_result_t zeEventPoolOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool) {
    return L0::Context::fromHandle(hContext)->openEventPoolIpcHandle(hIpc, phEventPool);
}

ze_result_t zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool) {
    return L0::EventPool::fromHandle(hEventPool)->closeIpcHandle();
}

ze_result_t zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendSignalEvent(hEvent, false);
}

ze_result_t zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendWaitOnEvents(numEvents, phEvents, nullptr, false, true, true, false, false, false);
}

ze_result_t zeEventHostSignal(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->hostSignal(false);
}

ze_result_t zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint64_t timeout) {
    return L0::Event::fromHandle(hEvent)->hostSynchronize(timeout);
}

ze_result_t zeEventQueryStatus(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->queryStatus();
}

ze_result_t zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendEventReset(hEvent);
}

ze_result_t zeEventHostReset(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->reset();
}

ze_result_t zeEventQueryKernelTimestamp(
    ze_event_handle_t hEvent,
    ze_kernel_timestamp_result_t *timestampType) {
    return L0::Event::fromHandle(hEvent)->queryKernelTimestamp(timestampType);
}

ze_result_t zeEventQueryKernelTimestampsExt(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_event_query_kernel_timestamps_results_ext_properties_t *pResults) {
    return L0::Event::fromHandle(hEvent)->queryKernelTimestampsExt(L0::Device::fromHandle(hDevice), pCount, pResults);
}

ze_result_t zeEventPoolPutIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeEventPoolGetContextHandle(
    ze_event_pool_handle_t hEventPool,
    ze_context_handle_t *phContext) {
    return L0::EventPool::fromHandle(hEventPool)->getContextHandle(phContext);
}

ze_result_t zeEventPoolGetFlags(
    ze_event_pool_handle_t hEventPool,
    ze_event_pool_flags_t *pFlags) {
    return L0::EventPool::fromHandle(hEventPool)->getFlags(pFlags);
}

ze_result_t zeEventGetEventPool(
    ze_event_handle_t hEvent,
    ze_event_pool_handle_t *phEventPool) {
    return L0::Event::fromHandle(hEvent)->getEventPool(phEventPool);
}

ze_result_t zeEventGetSignalScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pSignalScope) {
    return L0::Event::fromHandle(hEvent)->getSignalScope(pSignalScope);
}

ze_result_t zeEventGetWaitScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pWaitScope) {
    return L0::Event::fromHandle(hEvent)->getWaitScope(pWaitScope);
}
} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolCreate(
    ze_context_handle_t hContext,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool) {
    return L0::zeEventPoolCreate(
        hContext,
        desc,
        numDevices,
        phDevices,
        phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool) {
    return L0::zeEventPoolDestroy(
        hEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent) {
    return L0::zeEventCreate(
        hEventPool,
        desc,
        phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventDestroy(
    ze_event_handle_t hEvent) {
    return L0::zeEventDestroy(
        hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc) {
    return L0::zeEventPoolGetIpcHandle(
        hEventPool,
        phIpc);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool) {
    return L0::zeEventPoolOpenIpcHandle(
        hContext,
        hIpc,
        phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool) {
    return L0::zeEventPoolCloseIpcHandle(
        hEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::zeCommandListAppendSignalEvent(
        hCommandList,
        hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents) {
    return L0::zeCommandListAppendWaitOnEvents(
        hCommandList,
        numEvents,
        phEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventHostSignal(
    ze_event_handle_t hEvent) {
    return L0::zeEventHostSignal(
        hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint64_t timeout) {
    return L0::zeEventHostSynchronize(
        hEvent,
        timeout);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventQueryStatus(
    ze_event_handle_t hEvent) {
    return L0::zeEventQueryStatus(
        hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::zeCommandListAppendEventReset(
        hCommandList,
        hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventHostReset(
    ze_event_handle_t hEvent) {
    return L0::zeEventHostReset(
        hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventQueryKernelTimestamp(
    ze_event_handle_t hEvent,
    ze_kernel_timestamp_result_t *dstptr) {
    return L0::zeEventQueryKernelTimestamp(
        hEvent,
        dstptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventQueryKernelTimestampsExt(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_event_query_kernel_timestamps_results_ext_properties_t *pResults) {
    return L0::zeEventQueryKernelTimestampsExt(
        hEvent,
        hDevice,
        pCount,
        pResults);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendQueryKernelTimestamps(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents,
    void *dstptr,
    const size_t *pOffsets,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendQueryKernelTimestamps(
        hCommandList,
        numEvents,
        phEvents,
        dstptr,
        pOffsets,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolGetContextHandle(
    ze_event_pool_handle_t hEventPool,
    ze_context_handle_t *phContext) {
    return L0::zeEventPoolGetContextHandle(hEventPool, phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolGetFlags(
    ze_event_pool_handle_t hEventPool,
    ze_event_pool_flags_t *pFlags) {
    return L0::zeEventPoolGetFlags(hEventPool, pFlags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetEventPool(
    ze_event_handle_t hEvent,
    ze_event_pool_handle_t *phEventPool) {
    return L0::zeEventGetEventPool(hEvent, phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetSignalScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pSignalScope) {
    return L0::zeEventGetSignalScope(hEvent, pSignalScope);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetWaitScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pWaitScope) {
    return L0::zeEventGetWaitScope(hEvent, pWaitScope);
}
}
