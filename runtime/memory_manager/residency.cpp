/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/residency.h"

#include "runtime/os_interface/os_context.h"

using namespace NEO;

void ResidencyData::updateCompletionData(uint64_t newFenceValue, uint32_t contextId) {
    lastFenceValues[contextId] = newFenceValue;
}

uint64_t ResidencyData::getFenceValueForContextId(uint32_t contextId) {
    return lastFenceValues[contextId];
}
