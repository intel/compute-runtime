/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event/event.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCreate(
    ze_context_handle_t hContext,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool) {
    return L0::Context::fromHandle(hContext)->createEventPool(desc, numDevices, phDevices, phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool) {
    return L0::EventPool::fromHandle(hEventPool)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent) {
    return L0::EventPool::fromHandle(hEventPool)->createEvent(desc, phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventDestroy(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc) {
    return L0::EventPool::fromHandle(hEventPool)->getIpcHandle(phIpc);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool) {
    return L0::Context::fromHandle(hContext)->openEventPoolIpcHandle(hIpc, phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool) {
    return L0::EventPool::fromHandle(hEventPool)->closeIpcHandle();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendSignalEvent(hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendWaitOnEvents(numEvents, phEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSignal(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->hostSignal();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint64_t timeout) {
    return L0::Event::fromHandle(hEvent)->hostSynchronize(timeout);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryStatus(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->queryStatus();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendEventReset(hEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostReset(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->reset();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryKernelTimestamp(
    ze_event_handle_t hEvent,
    ze_kernel_timestamp_result_t *timestampType) {
    return L0::Event::fromHandle(hEvent)->queryKernelTimestamp(timestampType);
}