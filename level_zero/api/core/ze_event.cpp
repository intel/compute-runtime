/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeEventPoolCreate(
    ze_driver_handle_t hDriver,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool) {
    return L0::DriverHandle::fromHandle(hDriver)->createEventPool(desc, numDevices, phDevices, phEventPool);
}

__zedllexport ze_result_t __zecall
zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool) {
    return L0::EventPool::fromHandle(hEventPool)->destroy();
}

__zedllexport ze_result_t __zecall
zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent) {
    return L0::EventPool::fromHandle(hEventPool)->createEvent(desc, phEvent);
}

__zedllexport ze_result_t __zecall
zeEventDestroy(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->destroy();
}

__zedllexport ze_result_t __zecall
zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc) {
    return L0::EventPool::fromHandle(hEventPool)->getIpcHandle(phIpc);
}

__zedllexport ze_result_t __zecall
zeEventPoolOpenIpcHandle(
    ze_driver_handle_t hDriver,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool) {
    return L0::DriverHandle::fromHandle(hDriver)->openEventPoolIpcHandle(hIpc, phEventPool);
}

__zedllexport ze_result_t __zecall
zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool) {
    return L0::EventPool::fromHandle(hEventPool)->closeIpcHandle();
}

__zedllexport ze_result_t __zecall
zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendSignalEvent(hEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendWaitOnEvents(numEvents, phEvents);
}

__zedllexport ze_result_t __zecall
zeEventHostSignal(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->hostSignal();
}

__zedllexport ze_result_t __zecall
zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint32_t timeout) {
    return L0::Event::fromHandle(hEvent)->hostSynchronize(timeout);
}

__zedllexport ze_result_t __zecall
zeEventQueryStatus(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->queryStatus();
}

__zedllexport ze_result_t __zecall
zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendEventReset(hEvent);
}

__zedllexport ze_result_t __zecall
zeEventHostReset(
    ze_event_handle_t hEvent) {
    return L0::Event::fromHandle(hEvent)->reset();
}

__zedllexport ze_result_t __zecall
zeEventGetTimestamp(
    ze_event_handle_t hEvent,
    ze_event_timestamp_type_t timestampType,
    void *dstptr) {
    return L0::Event::fromHandle(hEvent)->getTimestamp(timestampType, dstptr);
}

} // extern "C"