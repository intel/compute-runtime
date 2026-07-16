/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList,
    uint32_t numRanges,
    const size_t *pRangeSizes,
    const void **pRanges,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeDeviceSystemBarrier(
    ze_device_handle_t hDevice);

ze_result_t ZE_APICALL zeCommandListHostSynchronize(
    ze_command_list_handle_t hCommandList,
    uint64_t timeout);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList,
    uint32_t numRanges,
    const size_t *pRangeSizes,
    const void **pRanges,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceSystemBarrier(
    ze_device_handle_t hDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListHostSynchronize(
    ze_command_list_handle_t hCommandList,
    uint64_t timeout);

} // extern "C"
