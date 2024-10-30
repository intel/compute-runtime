/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/driver_experimental/public/zex_common.h"
#include <level_zero/ze_api.h>

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexEventGetDeviceAddress(
    ze_event_handle_t event,
    uint64_t *completionValue,
    uint64_t *address);

// deprecated
ZE_APIEXPORT ze_result_t ZE_APICALL
zexCounterBasedEventCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    uint64_t *deviceAddress,
    uint64_t *hostAddress,
    uint64_t completionValue,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zexIntelAllocateNetworkInterrupt(ze_context_handle_t hContext, uint32_t &networkInterruptId);

ZE_APIEXPORT ze_result_t ZE_APICALL zexIntelReleaseNetworkInterrupt(ze_context_handle_t hContext, uint32_t networkInterruptId);

ZE_APIEXPORT ze_result_t ZE_APICALL zexCounterBasedEventCreate2(ze_context_handle_t hContext, ze_device_handle_t hDevice, const zex_counter_based_event_desc_t *desc, ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zexCounterBasedEventGetIpcHandle(ze_event_handle_t hEvent, zex_ipc_counter_based_event_handle_t *phIpc);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCounterBasedEventOpenIpcHandle(ze_context_handle_t hContext, zex_ipc_counter_based_event_handle_t hIpc, ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCounterBasedEventCloseIpcHandle(ze_event_handle_t hEvent);

} // namespace L0
