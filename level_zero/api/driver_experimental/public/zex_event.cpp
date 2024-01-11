/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_event.h"

#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/core/source/event/event.h"

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexEventGetDeviceAddress(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address) {
    auto eventObj = Event::fromHandle(event);

    if (!eventObj || !eventObj->isCounterBased() || !completionValue || !address) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *completionValue = eventObj->getInOrderExecSignalValueWithSubmissionCounter();

    auto deviceAlloc = eventObj->getInOrderExecDataAllocation();

    if (deviceAlloc) {
        *address = deviceAlloc->getGpuAddress() + eventObj->getInOrderAllocationOffset();
    } else {
        *address = 0;
    }

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCounterBasedEventCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, uint64_t *deviceAddress, uint64_t *hostAddress, uint64_t completionValue, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
