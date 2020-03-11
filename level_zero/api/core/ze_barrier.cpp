/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList,
    uint32_t numRanges,
    const size_t *pRangeSizes,
    const void **pRanges,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendMemoryRangesBarrier(numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents);
}

__zedllexport ze_result_t __zecall
zeDeviceSystemBarrier(
    ze_device_handle_t hDevice) {
    return L0::Device::fromHandle(hDevice)->systemBarrier();
}

} // extern "C"
