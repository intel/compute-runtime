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
zexEventGetDeviceAddress(
    ze_event_handle_t event,
    uint64_t *completionValue,
    uint64_t *address);

// deprecated
ze_result_t ZE_APICALL
zexCounterBasedEventCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    uint64_t *deviceAddress,
    uint64_t *hostAddress,
    uint64_t completionValue,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent);

ze_result_t ZE_APICALL zexIntelAllocateNetworkInterrupt(ze_context_handle_t hContext, uint32_t &networkInterruptId);

ze_result_t ZE_APICALL zexIntelReleaseNetworkInterrupt(ze_context_handle_t hContext, uint32_t networkInterruptId);

ze_result_t ZE_APICALL zexCounterBasedEventCreate2(ze_context_handle_t hContext, ze_device_handle_t hDevice, const zex_counter_based_event_desc_t *desc, ze_event_handle_t *phEvent);

ze_result_t ZE_APICALL zexCounterBasedEventGetIpcHandle(ze_event_handle_t hEvent, zex_ipc_counter_based_event_handle_t *phIpc);

ze_result_t ZE_APICALL zexCounterBasedEventOpenIpcHandle(ze_context_handle_t hContext, zex_ipc_counter_based_event_handle_t hIpc, ze_event_handle_t *phEvent);

ze_result_t ZE_APICALL zexCounterBasedEventCloseIpcHandle(ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zexDeviceGetAggregatedCopyOffloadIncrementValue(ze_device_handle_t hDevice, uint32_t *incrementValue);

} // namespace L0
