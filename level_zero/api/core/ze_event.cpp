/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event/event.h"
#include <level_zero/ze_api.h>

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCreate(
    ze_driver_handle_t hDriver,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool) {
    return L0::DriverHandle::fromHandle(hDriver)->createEventPool(desc, numDevices, phDevices, phEventPool);
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
    ze_driver_handle_t hDriver,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool) {
    return L0::DriverHandle::fromHandle(hDriver)->openEventPoolIpcHandle(hIpc, phEventPool);
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
    uint32_t timeout) {
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
zeEventGetTimestamp(
    ze_event_handle_t hEvent,
    ze_event_timestamp_type_t timestampType,
    void *dstptr) {
    return L0::Event::fromHandle(hEvent)->getTimestamp(timestampType, dstptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryKernelTimestampExt(
    ze_event_handle_t hEvent,
    ze_kernel_timestamp_result_t *dstptr) {
    return L0::Event::fromHandle(hEvent)->queryKernelTimestamp(dstptr);
}
} // extern "C"