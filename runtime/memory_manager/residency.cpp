/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/residency.h"

#include "runtime/os_interface/os_context.h"

using namespace OCLRT;

void ResidencyData::updateCompletionData(uint64_t newFenceValue, uint32_t contextId) {
    if (contextId + 1 > lastFenceValues.size()) {
        lastFenceValues.resize(contextId + 1);
    }
    lastFenceValues[contextId] = newFenceValue;
}

uint64_t ResidencyData::getFenceValueForContextId(uint32_t contextId) {
    return lastFenceValues[contextId];
}
