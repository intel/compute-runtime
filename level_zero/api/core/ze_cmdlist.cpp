/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList) {
    return L0::Context::fromHandle(hContext)->createCommandList(hDevice, desc, phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateImmediate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList) {
    return L0::Context::fromHandle(hContext)->createCommandListImmediate(hDevice, altdesc, phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListDestroy(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListClose(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->close();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListReset(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->reset();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWriteGlobalTimestamp(
    ze_command_list_handle_t hCommandList,
    uint64_t *dstptr,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendQueryKernelTimestamps(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents,
    void *dstptr,
    const size_t *pOffsets,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendQueryKernelTimestamps(numEvents, phEvents, dstptr, pOffsets, hSignalEvent, numWaitEvents, phWaitEvents);
}
