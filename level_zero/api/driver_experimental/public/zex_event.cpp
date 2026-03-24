/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_event.h"

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

ze_result_t ZE_APICALL
zexEventGetDeviceAddress(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address) {
    return Event::counterBasedGetDeviceAddress(event, completionValue, address);
}

ze_result_t ZE_APICALL
zexCounterBasedEventCreate2(ze_context_handle_t hContext, ze_device_handle_t hDevice, const zex_counter_based_event_desc_t *desc, ze_event_handle_t *phEvent) {
    const auto localDesc = desc ? desc : &defaultZexIntelCounterBasedEventDesc;

    const ze_event_counter_based_desc_t mappedDesc = {.stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
                                                      .pNext = localDesc->pNext,
                                                      .flags = static_cast<ze_event_counter_based_flags_t>(localDesc->flags),
                                                      .signal = localDesc->signalScope,
                                                      .wait = localDesc->waitScope};

    return Event::counterBasedCreate(hContext, hDevice, &mappedDesc, phEvent);
}

ze_result_t ZE_APICALL
zexCounterBasedEventCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, uint64_t *deviceAddress, uint64_t *hostAddress, uint64_t completionValue, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    constexpr uint32_t counterBasedFlags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;

    if (!desc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    zex_counter_based_event_external_sync_alloc_properties_t externalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES};
    externalSyncAllocProperties.completionValue = completionValue;
    externalSyncAllocProperties.deviceAddress = deviceAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = counterBasedFlags;
    counterBasedDesc.signalScope = desc->signal;
    counterBasedDesc.waitScope = desc->wait;
    counterBasedDesc.pNext = desc->pNext;

    if (deviceAddress && hostAddress) {
        externalSyncAllocProperties.pNext = desc->pNext;
        counterBasedDesc.pNext = &externalSyncAllocProperties;
    }

    return L0::zexCounterBasedEventCreate2(hContext, hDevice, &counterBasedDesc, phEvent);
}

ze_result_t ZE_APICALL zexIntelAllocateNetworkInterrupt(ze_context_handle_t hContext, uint32_t &networkInterruptId) {
    auto context = L0::Context::fromHandle(toInternalType(hContext));

    if (!context) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (!context->getDriverHandle()->getMemoryManager()->allocateInterrupt(networkInterruptId, context->rootDeviceIndices[0])) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zexIntelReleaseNetworkInterrupt(ze_context_handle_t hContext, uint32_t networkInterruptId) {
    auto context = L0::Context::fromHandle(toInternalType(hContext));

    if (!context || !context->getDriverHandle()->getMemoryManager()->releaseInterrupt(networkInterruptId, context->rootDeviceIndices[0])) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zexCounterBasedEventGetIpcHandle(ze_event_handle_t hEvent, zex_ipc_counter_based_event_handle_t *phIpc) {
    auto ret = Event::counterBasedGetIpcHandle(hEvent, reinterpret_cast<ze_ipc_event_counter_based_handle_t *>(phIpc));
    return ret;
}

ze_result_t ZE_APICALL zexCounterBasedEventOpenIpcHandle(ze_context_handle_t hContext, zex_ipc_counter_based_event_handle_t hIpc, ze_event_handle_t *phEvent) {
    return Event::counterBasedOpenIpcHandle(hContext, reinterpret_cast<ze_ipc_event_counter_based_handle_t &>(hIpc), phEvent);
}

ze_result_t ZE_APICALL zexCounterBasedEventCloseIpcHandle(ze_event_handle_t hEvent) {
    return Event::fromHandle(hEvent)->destroy();
}

ze_result_t ZE_APICALL zexDeviceGetAggregatedCopyOffloadIncrementValue(ze_device_handle_t hDevice, uint32_t *incrementValue) {
    return Event::counterBasedGetIncrementValue(hDevice, incrementValue);
}

} // namespace L0
